MessageIdTypedef=DWORD

SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
    Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
    Warning=0x2:STATUS_SEVERITY_WARNING
    Error=0x3:STATUS_SEVERITY_ERROR
    )


FacilityNames=(System=0x0:FACILITY_SYSTEM
    Runtime=0x2:FACILITY_RUNTIME
    Stubs=0x3:FACILITY_STUBS
    Io=0x4:FACILITY_IO_ERROR_CODE
)

LanguageNames=(English=0x409:MSG00409)


MessageId=0x1
Severity=Error
Facility=Runtime
SymbolicName=SVC_ERROR
Language=English
An error has occurred (%2).
.

MessageId=0x2
Severity=Warning
Facility=Runtime
SymbolicName=OLED_AWAKE_WARNING
Language=English
%1 has issued a warning. The error message is: %2.
.

MessageId=0x2
Severity=Informational
Facility=Runtime
SymbolicName=OLED_AWAKE_INFORMATION
Language=English
%1 log info (%2).
.
