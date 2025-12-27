#include "AlakazamSettings.h"

UAlakazamSettings::UAlakazamSettings()
{
}

UAlakazamSettings* UAlakazamSettings::Get()
{
	return GetMutableDefault<UAlakazamSettings>();
}
