#include "ChoicesXml.h"
#import <Foundation/Foundation.h>

bool readChoiceChanges(const char* path, std::vector<ChoiceChange>& changes)
{
	@autoreleasepool
	{
		NSArray* array = [NSArray arrayWithContentsOfFile: [NSString stringWithUTF8String: path]];
		if (![array isKindOfClass: [NSArray class]])
			return false;
		
		std::vector<ChoiceChange> result;
		for (id item in array)
		{
			ChoiceChange change;
			
			if ([item isKindOfClass: [NSString class]])
			{
				change.identifier = [item UTF8String];
				change.toggle = true;
			}
			else if ([item isKindOfClass: [NSDictionary class]])
			{
				id identifier = item[@"choiceIdentifier"];
				id attribute = item[@"choiceAttribute"];
				id setting = item[@"attributeSetting"];
				
				if (![identifier isKindOfClass: [NSString class]] || ![attribute isKindOfClass: [NSString class]])
					return false;
				change.identifier = [identifier UTF8String];
				change.attribute = [attribute UTF8String];
				if ([setting isKindOfClass: [NSString class]])
					change.stringValue = [setting UTF8String];
				else if ([setting isKindOfClass: [NSNumber class]])
					change.value = [setting longValue];
				else
					return false;
			}
			else
				return false;
			result.push_back(change);
		}
		changes.insert(changes.end(), result.begin(), result.end());
	}
	return true;
}
