#include "..\dll\pch.h"
#include "libevent.h"
#include "lib.h"

extern "C" const char* __stdcall LibInfo() { // ��������дӦ�������Ϣ
return (const char *)R"(
{
    "ver": 1,
    "AppID": "cn.coorg.coolib",
    "priority": 0,
    "AppVer": "1.0.0",
    "lib": [],
    "LibAPI": [
        {
            "func": "CheckLibA",
            "?": "@@YG_NPADPAD@Z"
        },
        {
            "func": "VersionMatch",
            "?": "@@YG_NPADPAD@Z"
        }
    ]
}
)";
}

extern "C" int32_t __stdcall LibCallback(bool a, const char* b) { // �������¼����������ʼ������API�ӿ�
	return 0;
}

extern "C" int32_t __stdcall AppCallback() { // Ӧ�������¼����������ʼ��Ӧ��
	return 0;
}

extern "C" int32_t __stdcall DisableCallback() { // Ӧ��ж���¼������������Ӧ��
	return 0;
}

extern "C" int32_t __stdcall ExitCallback(const char* a) { // ��ж���¼��������������
	return 0;
}

extern "C" int32_t __stdcall CheckLibA(const char* LibAppID, const char* LibVer) {
	for (cLibInfo& i : libList) {
		if (i.name == LibAppID && versionMatch(LibVer, i.j["AppVer"].get<std::string>())) {
			return true;
		}
	}
	return false;
}

extern "C" int32_t __stdcall VersionMatch(const char* version, const char* range) {
	return versionMatch(version, range);
}
