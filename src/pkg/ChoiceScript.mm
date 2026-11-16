#include "ChoiceScript.h"
#include <dlfcn.h>
#include <cstring>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <unistd.h>
#include <crt_externs.h>
#include <syslog.h>
#import <Foundation/Foundation.h>

// The public JavaScriptCore C API (JSBase.h, JSValueRef.h, JSObjectRef.h, JSStringRef.h),
// declared here because the library is loaded with dlopen.
extern "C"
{
	typedef const struct OpaqueJSContext* JSContextRef;
	typedef struct OpaqueJSContext* JSGlobalContextRef;
	typedef struct OpaqueJSString* JSStringRef;
	typedef const struct OpaqueJSValue* JSValueRef;
	typedef struct OpaqueJSValue* JSObjectRef;
	typedef struct OpaqueJSClass* JSClassRef;
	typedef JSValueRef (*JSObjectCallAsFunctionCallback)(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
			size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
}

namespace
{
	struct JSApi
	{
		JSGlobalContextRef (*GlobalContextCreate)(JSClassRef);
		void (*GlobalContextRelease)(JSGlobalContextRef);
		JSObjectRef (*ContextGetGlobalObject)(JSContextRef);
		JSStringRef (*StringCreateWithUTF8CString)(const char*);
		void (*StringRelease)(JSStringRef);
		size_t (*StringGetMaximumUTF8CStringSize)(JSStringRef);
		size_t (*StringGetUTF8CString)(JSStringRef, char*, size_t);
		JSValueRef (*EvaluateScript)(JSContextRef, JSStringRef, JSObjectRef, JSStringRef, int, JSValueRef*);
		bool (*ValueToBoolean)(JSContextRef, JSValueRef);
		JSStringRef (*ValueToStringCopy)(JSContextRef, JSValueRef, JSValueRef*);
		JSObjectRef (*ObjectMakeFunctionWithCallback)(JSContextRef, JSStringRef, JSObjectCallAsFunctionCallback);
		void (*ObjectSetProperty)(JSContextRef, JSObjectRef, JSStringRef, JSValueRef, unsigned, JSValueRef*);
		JSValueRef (*ValueMakeBoolean)(JSContextRef, bool);
		JSValueRef (*ValueMakeNumber)(JSContextRef, double);
		JSValueRef (*ValueMakeString)(JSContextRef, JSStringRef);
		JSValueRef (*ValueMakeUndefined)(JSContextRef);
		bool (*ValueIsBoolean)(JSContextRef, JSValueRef);
	};

	const JSApi* loadApi()
	{
		static JSApi api;
		static bool tried = false, ok = false;

		if (tried)
			return ok ? &api : nullptr;
		tried = true;

		void* lib = dlopen("/System/Library/Frameworks/JavaScriptCore.framework/JavaScriptCore", RTLD_NOW | RTLD_LOCAL);
		if (!lib)
			lib = dlopen("JavaScriptCore.framework/JavaScriptCore", RTLD_NOW | RTLD_LOCAL); // honours DYLD_FRAMEWORK_PATH
		if (!lib)
			return nullptr;

#define LOAD(field, symbol) \
		*(void**) &api.field = dlsym(lib, symbol); \
		if (!api.field) return nullptr;
		LOAD(GlobalContextCreate, "JSGlobalContextCreate")
		LOAD(GlobalContextRelease, "JSGlobalContextRelease")
		LOAD(ContextGetGlobalObject, "JSContextGetGlobalObject")
		LOAD(StringCreateWithUTF8CString, "JSStringCreateWithUTF8CString")
		LOAD(StringRelease, "JSStringRelease")
		LOAD(StringGetMaximumUTF8CStringSize, "JSStringGetMaximumUTF8CStringSize")
		LOAD(StringGetUTF8CString, "JSStringGetUTF8CString")
		LOAD(EvaluateScript, "JSEvaluateScript")
		LOAD(ValueToBoolean, "JSValueToBoolean")
		LOAD(ValueToStringCopy, "JSValueToStringCopy")
		LOAD(ObjectMakeFunctionWithCallback, "JSObjectMakeFunctionWithCallback")
		LOAD(ObjectSetProperty, "JSObjectSetProperty")
		LOAD(ValueMakeBoolean, "JSValueMakeBoolean")
		LOAD(ValueMakeNumber, "JSValueMakeNumber")
		LOAD(ValueMakeString, "JSValueMakeString")
		LOAD(ValueMakeUndefined, "JSValueMakeUndefined")
		LOAD(ValueIsBoolean, "JSValueIsBoolean")
#undef LOAD
		ok = true;
		return &api;
	}

	// A JavaScript string literal for the text
	std::string quote(const std::string& text)
	{
		std::string rv = "\"";
		static const char* hex = "0123456789abcdef";

		for (unsigned char c : text)
		{
			if (c == '"' || c == '\\')
			{
				rv += '\\';
				rv += c;
			}
			else if (c < 0x20)
			{
				rv += "\\u00";
				rv += hex[c >> 4];
				rv += hex[c & 15];
			}
			else
				rv += c;
		}
		return rv + "\"";
	}

	// A property list value as JSON-compatible Foundation objects (dates, data and
	// other types without a JSON form become strings)
	id jsonSafe(id value)
	{
		if ([value isKindOfClass: [NSDictionary class]])
		{
			NSMutableDictionary* rv = [NSMutableDictionary dictionary];
			for (id key in value)
				rv[[key description]] = jsonSafe(value[key]);
			return rv;
		}
		if ([value isKindOfClass: [NSArray class]])
		{
			NSMutableArray* rv = [NSMutableArray array];
			for (id item in value)
				[rv addObject: jsonSafe(item)];
			return rv;
		}
		if ([value isKindOfClass: [NSString class]] || [value isKindOfClass: [NSNumber class]])
			return value;
		return [value description];
	}

	// The property list at path (or the Info.plist of the bundle there) as JSON text; empty if unreadable
	std::string plistAsJSON(const std::string& path, bool bundle)
	{
		std::string rv;

		@autoreleasepool
		{
			NSString* file = [NSString stringWithUTF8String: path.c_str()];
			if (bundle)
				file = [file stringByAppendingPathComponent: @"Contents/Info.plist"];

			NSDictionary* plist = [NSDictionary dictionaryWithContentsOfFile: file];
			if (plist != nil)
			{
				NSData* json = [NSJSONSerialization dataWithJSONObject: jsonSafe(plist) options: 0 error: nil];
				if (json != nil)
					rv.assign((const char*) [json bytes], [json length]);
			}
		}
		return rv;
	}

	// SystemVersion.plist of the volume mounted at root as a JavaScript object literal
	std::string systemVersion(const std::string& root)
	{
		std::string json = plistAsJSON(root + "/System/Library/CoreServices/SystemVersion.plist", false);
		return json.empty() ? "{}" : json;
	}

	std::string stringArgument(JSContextRef ctx, const JSValueRef args[], size_t argc, JSValueRef* exception)
	{
		const JSApi* api = loadApi();
		std::string rv;

		if (argc >= 1)
		{
			if (JSStringRef str = api->ValueToStringCopy(ctx, args[0], exception))
			{
				rv.assign(api->StringGetMaximumUTF8CStringSize(str), '\0');
				api->StringGetUTF8CString(str, &rv[0], rv.size());
				rv.resize(strlen(rv.c_str()));
				api->StringRelease(str);
			}
		}
		return rv;
	}

	// system.sysctl(name): a number for an integer value, a string otherwise
	JSValueRef sysctlValue(JSContextRef ctx, JSObjectRef, JSObjectRef, size_t argc, const JSValueRef args[], JSValueRef* exception)
	{
		const JSApi* api = loadApi();
		std::string name = stringArgument(ctx, args, argc, exception);
		size_t size = 0;

		if (name.empty() || ::sysctlbyname(name.c_str(), nullptr, &size, nullptr, 0) != 0)
			return api->ValueMakeUndefined(ctx);

		std::string buffer(size, '\0');
		if (::sysctlbyname(name.c_str(), &buffer[0], &size, nullptr, 0) != 0)
			return api->ValueMakeUndefined(ctx);

		// The value's type is not known here: 4 and 8 byte values are taken as integers
		if (size == sizeof(int32_t))
		{
			int32_t v;
			memcpy(&v, buffer.data(), sizeof(v));
			return api->ValueMakeNumber(ctx, v);
		}
		if (size == sizeof(int64_t))
		{
			int64_t v;
			memcpy(&v, buffer.data(), sizeof(v));
			return api->ValueMakeNumber(ctx, (double) v);
		}
		buffer.resize(strnlen(buffer.c_str(), size));
		JSStringRef str = api->StringCreateWithUTF8CString(buffer.c_str());
		JSValueRef value = api->ValueMakeString(ctx, str);
		api->StringRelease(str);
		return value;
	}

	JSValueRef jsonString(JSContextRef ctx, const std::string& json)
	{
		const JSApi* api = loadApi();

		if (json.empty())
			return api->ValueMakeUndefined(ctx);
		JSStringRef str = api->StringCreateWithUTF8CString(json.c_str());
		JSValueRef value = api->ValueMakeString(ctx, str);
		api->StringRelease(str);
		return value;
	}

	// __plistJSON(path) and __bundleJSON(path): the plist as a JSON string, undefined if unreadable
	JSValueRef plistJSON(JSContextRef ctx, JSObjectRef, JSObjectRef, size_t argc, const JSValueRef args[], JSValueRef* exception)
	{
		return jsonString(ctx, plistAsJSON(stringArgument(ctx, args, argc, exception), false));
	}

	JSValueRef bundleJSON(JSContextRef ctx, JSObjectRef, JSObjectRef, size_t argc, const JSValueRef args[], JSValueRef* exception)
	{
		return jsonString(ctx, plistAsJSON(stringArgument(ctx, args, argc, exception), true));
	}

	// system.log(string): an Installer log entry with the JS: prefix
	JSValueRef logEntry(JSContextRef ctx, JSObjectRef, JSObjectRef, size_t argc, const JSValueRef args[], JSValueRef* exception)
	{
		syslog(LOG_INSTALL | LOG_INFO, "JS: %s", stringArgument(ctx, args, argc, exception).c_str());
		return loadApi()->ValueMakeUndefined(ctx);
	}

	// The environment as a JavaScript object literal
	std::string environment()
	{
		std::string rv = "{";
		for (char** e = *_NSGetEnviron(); *e != nullptr; e++)
		{
			const char* eq = strchr(*e, '=');
			if (eq == nullptr)
				continue;
			if (rv.length() > 1)
				rv += ",";
			rv += quote(std::string(*e, eq - *e)) + ":" + quote(eq + 1);
		}
		return rv + "}";
	}

	JSValueRef fileExistsAtPath(JSContextRef ctx, JSObjectRef, JSObjectRef, size_t argc, const JSValueRef args[], JSValueRef* exception)
	{
		const JSApi* api = loadApi();
		struct stat st;
		bool exists = false;

		if (argc >= 1)
		{
			JSStringRef str = api->ValueToStringCopy(ctx, args[0], exception);
			if (str != nullptr)
			{
				std::string path(api->StringGetMaximumUTF8CStringSize(str), '\0');
				api->StringGetUTF8CString(str, &path[0], path.size());
				path.resize(strlen(path.c_str()));
				api->StringRelease(str);
				exists = ::stat(path.c_str(), &st) == 0;
			}
		}
		return api->ValueMakeBoolean(ctx, exists);
	}
}

struct ChoiceScript::Impl
{
	const JSApi* api;
	JSGlobalContextRef ctx = nullptr;

	// Returns false and the exception text if the script throws
	bool run(const std::string& source, JSValueRef* value, std::string& error)
	{
		JSStringRef script = api->StringCreateWithUTF8CString(source.c_str());
		JSValueRef exception = nullptr;
		JSValueRef result = api->EvaluateScript(ctx, script, nullptr, nullptr, 1, &exception);

		api->StringRelease(script);
		if (exception != nullptr)
		{
			error = "JavaScript error";
			if (JSStringRef text = api->ValueToStringCopy(ctx, exception, nullptr))
			{
				std::string msg(api->StringGetMaximumUTF8CStringSize(text), '\0');
				api->StringGetUTF8CString(text, &msg[0], msg.size());
				msg.resize(strlen(msg.c_str()));
				api->StringRelease(text);
				error = msg;
			}
			return false;
		}
		if (value)
			*value = result;
		return true;
	}

	std::string runString(const std::string& source)
	{
		JSValueRef value = nullptr;
		std::string error, rv;

		if (!run(source, &value, error) || value == nullptr)
			return rv;
		if (JSStringRef text = api->ValueToStringCopy(ctx, value, nullptr))
		{
			rv.assign(api->StringGetMaximumUTF8CStringSize(text), '\0');
			api->StringGetUTF8CString(text, &rv[0], rv.size());
			rv.resize(strlen(rv.c_str()));
			api->StringRelease(text);
		}
		return rv;
	}
};

ChoiceScript::ChoiceScript() : m_impl(new Impl) {}

ChoiceScript::~ChoiceScript()
{
	if (m_impl && m_impl->ctx)
		m_impl->api->GlobalContextRelease(m_impl->ctx);
}

std::unique_ptr<ChoiceScript> ChoiceScript::create(const std::string& targetMountPoint)
{
	const JSApi* api = loadApi();
	if (api == nullptr)
		return nullptr;

	std::unique_ptr<ChoiceScript> script(new ChoiceScript);
	script->m_impl->api = api;
	script->m_impl->ctx = api->GlobalContextCreate(nullptr);
	if (script->m_impl->ctx == nullptr)
		return nullptr;

	JSGlobalContextRef ctx = script->m_impl->ctx;
	static const struct { const char* name; JSObjectCallAsFunctionCallback callback; } natives[] = {
		{ "__fileExistsAtPath", fileExistsAtPath },
		{ "__sysctl", sysctlValue },
		{ "__plistJSON", plistJSON },
		{ "__bundleJSON", bundleJSON },
		{ "__log", logEntry },
	};
	for (const auto& native : natives)
	{
		JSStringRef name = api->StringCreateWithUTF8CString(native.name);
		JSObjectRef fn = api->ObjectMakeFunctionWithCallback(ctx, name, native.callback);
		api->ObjectSetProperty(ctx, api->ContextGetGlobalObject(ctx), name, fn, 0, nullptr);
		api->StringRelease(name);
	}

	struct statfs fs;
	double availableKilobytes = 0;
	if (::statfs(targetMountPoint.c_str(), &fs) == 0)
		availableKilobytes = (double) fs.f_bavail * fs.f_bsize / 1024;

	std::string setup =
		"var system = {\n"
		"  version: " + systemVersion("/") + ",\n"
		"  compareVersions: function (a, b) {\n"
		"    var x = String(a).split('.'), y = String(b).split('.');\n"
		"    for (var i = 0; i < Math.max(x.length, y.length); i++) {\n"
		"      var m = parseInt(x[i] || '0', 10) || 0, n = parseInt(y[i] || '0', 10) || 0;\n"
		"      if (m != n) return m < n ? -1 : 1;\n"
		"    }\n"
		"    return 0;\n"
		"  },\n"
		"  files: {\n"
		"    fileExistsAtPath: __fileExistsAtPath,\n"
		"    plistAtPath: function (p) { var j = __plistJSON(p); return j === undefined ? null : JSON.parse(j); },\n"
		"    bundleAtPath: function (p) { var j = __bundleJSON(p); return j === undefined ? null : JSON.parse(j); }\n"
		"  },\n"
		"  sysctl: __sysctl,\n"
		"  propertiesOf: function (o) { var r = []; for (var k in o) r.push(k); return r; },\n"
		// The package carries no localized strings that could be looked up, so every key is missing
		"  localizedString: function (key) { return null; },\n"
		"  localizedStringWithFormat: function (key) { return null; },\n"
		"  env: " + environment() + ",\n"
		"  applications: { fromIdentifier: function (identifier) { return []; } },\n"
		"  log: __log\n"
		"};\n"
		"var my = { target: { mountpoint: " + quote(targetMountPoint) + ", systemVersion: " + systemVersion(targetMountPoint) + ",\n"
		"                     availableKilobytes: " + std::to_string((long long) availableKilobytes) + " },\n"
		"           result: {} };\n"
		"var choices = {};\n";
	std::string error;
	if (!script->m_impl->run(setup, nullptr, error))
		return nullptr;
	return script;
}

bool ChoiceScript::addScript(const std::string& source, std::string& error)
{
	return m_impl->run(source, nullptr, error);
}

void ChoiceScript::setChoices(const std::vector<ChoiceState>& choices)
{
	std::string source = "choices = {";
	std::string error;

	for (size_t i = 0; i < choices.size(); i++)
	{
		const ChoiceState& c = choices[i];
		// 1 and 0, not booleans: the Installer's values compare equal to true/false but are not of type boolean
		source += (i ? "," : "") + quote(c.id) + ":{selected:" + (c.selected ? "1" : "0")
				+ ",enabled:" + (c.enabled ? "1" : "0") + ",visible:" + (c.visible ? "1" : "0")
				+ ",title:" + quote(c.title) + ",description:" + quote(c.description)
				+ ",packageUpgradeAction:" + quote(c.packageUpgradeAction) + ",packages:[";
		for (size_t j = 0; j < c.packages.size(); j++)
			source += (j ? "," : "") + std::string("{identifier:") + quote(c.packages[j].identifier) + ",version:" + quote(c.packages[j].version) + "}";
		source += "]}";
	}
	source += "};";
	m_impl->run(source, nullptr, error);
}

bool ChoiceScript::evaluate(const std::string& expression, bool& result, std::string& error)
{
	JSValueRef value = nullptr;

	if (!m_impl->run("(" + expression + "\n)", &value, error))
		return false;
	// Only the boolean true counts; any other value (a string, a number) is false
	result = m_impl->api->ValueIsBoolean(m_impl->ctx, value) && m_impl->api->ValueToBoolean(m_impl->ctx, value);
	return true;
}

std::string ChoiceScript::resultType()
{
	return m_impl->runString("typeof my.result.type == 'string' ? my.result.type : ''");
}

std::string ChoiceScript::resultMessage()
{
	return m_impl->runString("typeof my.result.message == 'string' ? my.result.message : ''");
}
