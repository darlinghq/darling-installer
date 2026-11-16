#include "ChoiceScript.h"
#include <dlfcn.h>
#include <cstring>
#include <sys/stat.h>
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

	// SystemVersion.plist of the volume mounted at root as a JavaScript object literal
	std::string systemVersion(const std::string& root)
	{
		static const char* keys[] = { "ProductName", "ProductVersion", "ProductBuildVersion", "ProductUserVisibleVersion" };
		std::string rv = "{";

		@autoreleasepool
		{
			NSString* path = [[NSString stringWithUTF8String: root.c_str()] stringByAppendingPathComponent: @"System/Library/CoreServices/SystemVersion.plist"];
			NSDictionary* plist = [NSDictionary dictionaryWithContentsOfFile: path];

			for (const char* key : keys)
			{
				id value = plist[[NSString stringWithUTF8String: key]];
				if ([value isKindOfClass: [NSString class]])
				{
					if (rv.length() > 1)
						rv += ",";
					rv += std::string(key) + ":" + quote([value UTF8String]);
				}
			}
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
	JSStringRef name = api->StringCreateWithUTF8CString("__fileExistsAtPath");
	JSObjectRef fn = api->ObjectMakeFunctionWithCallback(ctx, name, fileExistsAtPath);
	api->ObjectSetProperty(ctx, api->ContextGetGlobalObject(ctx), name, fn, 0, nullptr);
	api->StringRelease(name);

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
		"  files: { fileExistsAtPath: __fileExistsAtPath },\n"
		"  log: function (message) { }\n"
		"};\n"
		"var my = { target: { mountpoint: " + quote(targetMountPoint) + ", systemVersion: " + systemVersion(targetMountPoint) + " },\n"
		"           result: { type: '', message: '' } };\n"
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
		source += (i ? "," : "") + quote(c.id) + ":{selected:" + (c.selected ? "true" : "false")
				+ ",enabled:" + (c.enabled ? "true" : "false") + ",visible:" + (c.visible ? "true" : "false") + "}";
	}
	source += "};";
	m_impl->run(source, nullptr, error);
}

bool ChoiceScript::evaluate(const std::string& expression, bool& result, std::string& error)
{
	JSValueRef value = nullptr;

	if (!m_impl->run("(" + expression + "\n)", &value, error))
		return false;
	result = m_impl->api->ValueToBoolean(m_impl->ctx, value);
	return true;
}

std::string ChoiceScript::resultType()
{
	return m_impl->runString("String(my.result.type)");
}

std::string ChoiceScript::resultMessage()
{
	return m_impl->runString("String(my.result.message)");
}
