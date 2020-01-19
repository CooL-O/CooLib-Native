// cqevent.cpp ����CQ�¼�
#include "pch.h"

#include "dll.h"
#include "cqevent.h"
#include "cqapi.h"

using json = nlohmann::json;

std::vector<cLibInfo> libList; // �����

extern "C" const char* __stdcall AppInfo()
{
	return "9,cn.coorg.coolib";
}

extern "C" int32_t __stdcall Initialize(int32_t AuthCode)
{
	cq::AuthCode = AuthCode;
	cq::__init_api();
	return 0;
}

extern "C" const char* __stdcall LibInfo() {
	return "{\"ver\":1,\"AppID\":\"cn.coorg.coolib\",\"priority\":0,\"AppVer\":\"1.0.0\"}";
}

extern "C" int32_t __stdcall LibCallback(bool a, const char* b) {
	return 0;
}

extern "C" int32_t __stdcall AppCallback() {
	return 0;
}

extern "C" int32_t __stdcall DisableCallback() {
	return 0;
}

extern "C" int32_t __stdcall ExitCallback(const char* a) {
	return 0;
}

extern "C" int32_t __stdcall _eventStartup()
{
	clock_t timeStart = clock();
	// ��ȡDLL·��
	char dllPath[MAX_PATH];
	GetModuleFileNameA(_hModule, dllPath, MAX_PATH);
	PathRemoveFileSpecA(dllPath);

	std::vector<cLibInfo> tlibList; // ��ʱ�����

	// ����������
	char szFilePath[MAX_PATH];
	HANDLE hListFile;
	WIN32_FIND_DATA FindFileData;
	lstrcpyA(szFilePath, dllPath);
	lstrcatA(szFilePath, "\\*.*");
	hListFile = FindFirstFileA(szFilePath, &FindFileData); // �������
	if (hListFile != INVALID_HANDLE_VALUE) {
		do {
			HMODULE hlib;
			std::string path;
			path = path + dllPath + "\\" + FindFileData.cFileName;
			hlib = LoadLibraryA(path.c_str()); // ����������
			if (hlib == NULL) { // ���Ϸ�
				FreeLibrary(hlib);
				continue;
			}
			typedef const char* (__stdcall* libInfoProc)();
			libInfoProc hlibInfoProc = (libInfoProc)GetProcAddress(hlib, "LibInfo"); // ��ȡLib��Ϣ
			if (hlibInfoProc == NULL) { // ��Lib
				FreeLibrary(hlib);
				continue;
			}
			json j = json::parse(hlibInfoProc());
			cLibInfo i(hlib, path, j);
			tlibList.push_back(i);
		} while (FindNextFileA(hListFile, &FindFileData));
	}

	// ������
	std::sort(tlibList.begin(), tlibList.end(), upsort);

	// ��������
	for (cLibInfo &i : tlibList) {
		if (i.j["ver"] == 1) {
			json bj;
			bj["s"] = true;
			std::vector<std::string> bmissLib;
			std::map<std::string, std::string> libPath;
			if (!i.j["lib"].empty()) {
				for (json::iterator libName = i.j["lib"].begin(); libName != i.j["lib"].end(); ++libName) { // ����Lib�����
					if (libName->empty()) continue;
					std::vector<cLibInfo>::iterator rlib = libFind(tlibList.begin(), tlibList.end(), libName.key());
					if (rlib == tlibList.end()) { // ������
						std::string errorInfo;
						errorInfo = errorInfo + "��� " + i.name + " ������ " + libName.key() + " ��ʧ�򲻴��ڣ���ȷ��ָ������Ѿ���װ�����á�";
						cq::CQ_addLog_Error("CooLib-Native", errorInfo.c_str());
						bj["s"] = false;
						bmissLib.push_back(libName.key());
						continue;
					}
					if (!rlib->loaded) { // δ����
						std::string errorInfo;
						errorInfo = errorInfo + "��� " + i.name + " ������ " + libName.key() + " δ��ȷ���أ���ȷ�ϲ�����ȼ�������ȷ��priority�ֶΣ���";
						cq::CQ_addLog_Error("CooLib-Native", errorInfo.c_str());
						bj["s"] = false;
						bmissLib.push_back(libName.key());
						continue;
					}
					if (!versionMatch(libName.value().get<std::string>(), rlib->j["AppVer"].get<std::string>())) { // �汾δƥ��
						std::string errorInfo;
						errorInfo = errorInfo + "��� " + i.name + " ������ " + libName.key() + " �汾����ȷ��汾���Ϸ�";
						errorInfo = errorInfo + "��required \"" + libName.value().get<std::string>() + "\" => installed \"" + rlib->j["AppVer"].get<std::string>() + "\"�����밲װ��ȷ�İ汾��";
						cq::CQ_addLog_Error("CooLib-Native", errorInfo.c_str());
						bj["s"] = false;
						bmissLib.push_back(libName.key());
						continue;
					}
					libPath[rlib->name] = rlib->path;
				}
				if (!bmissLib.empty()) bj["missLib"] = bmissLib;
				bj["libPath"] = libPath;
			}
			typedef int32_t(__stdcall* libCallbackProc)(bool, const char*);
			libCallbackProc hlibCallbackProc = (libCallbackProc)GetProcAddress(i.hlib, "LibCallback"); // Lib Callback
			if (hlibCallbackProc != NULL) {
				if (hlibCallbackProc(bmissLib.empty(), bj.dump().c_str()) == 0) { // ��������
					i.loaded = true;
				}
			}
		}
	}

	// ����
	for (cLibInfo &i : tlibList) {
		if (i.loaded) {
			libList.push_back(i);
		}
		else {
			FreeLibrary(i.hlib); // �ͷ��ļ�
		}
	}

	clock_t timeEnd = clock();
	std::string debugInfo;
	debugInfo = debugInfo + "Loaded.(" + std::to_string(libList.size()) + " succeed, " + std::to_string(timeEnd - timeStart) + "ms)";
	cq::CQ_addLog_Debug("CooLib-Native", debugInfo.c_str());

	return 0;
}

extern "C" int32_t __stdcall _eventEnable() {

	// ֪ͨ�¼�
	for (cLibInfo &i : libList) {
		typedef int32_t(__stdcall* appCallbackProc)();
		appCallbackProc happCallbackProc = (appCallbackProc)GetProcAddress(i.hlib, "AppCallback"); // App Callback
		if (happCallbackProc != NULL) {
			appCallbackProc(); // �����¼�
		}
	}

	return 0;
}

extern "C" int32_t __stdcall _eventDisable() {

	// ��������
	std::sort(libList.begin(), libList.end(), downsort);

	// ֪ͨ�¼�
	for (cLibInfo &i : libList) {
		typedef int32_t(__stdcall* disableCallbackProc)();
		disableCallbackProc hdisableCallbackProc = (disableCallbackProc)GetProcAddress(i.hlib, "DisableCallback"); // App Callback
		if (hdisableCallbackProc != NULL) {
			disableCallbackProc(); // �����¼�
		}
	}

	return 0;
}

extern "C" int32_t __stdcall _eventExit()
{
	// ֪ͨ�¼�
	for (cLibInfo &i : libList) {
		std::vector<std::string> bloadLib;
		for (std::string libName : i.j["lib"]) { // ����Lib�����
			if (!libName.empty()) {
				std::vector<cLibInfo>::iterator rlib = libFind(libList.begin(), libList.end(), libName);
				if (rlib->loaded) { // �Ѽ���
					bloadLib.push_back(rlib->name);
				}
				else { // δ����
					std::string errorInfo;
					errorInfo = errorInfo + "��� " + i.name + " ������ " + libName + " δ��ȷ���أ���ȷ�ϲ�����ȼ�������ȷ��priority�ֶΣ���";
					cq::CQ_addLog_Error("CooLib-Native", errorInfo.c_str());
				}
			}
		}
		json bj;
		bj["loadLib"] = bloadLib;
		typedef int32_t(__stdcall* exitCallbackProc)(const char*);
		exitCallbackProc hexitCallbackProc = (exitCallbackProc)GetProcAddress(i.hlib, "ExitCallback"); // App Callback
		if (hexitCallbackProc != NULL) {
			exitCallbackProc(bj.dump().c_str()); // �����¼�
		}
		i.loaded = false;
		FreeLibrary(i.hlib); // �ͷŲ��
	}

	return 0;
}

bool upsort(cLibInfo i, cLibInfo j) {
	return i.priority < j.priority;
}

bool downsort(cLibInfo i, cLibInfo j) {
	return i.priority > j.priority;
}

std::vector<cLibInfo>::iterator libFind(std::vector<cLibInfo>::iterator _First, const std::vector<cLibInfo>::iterator _Last, const std::string& _Val) {
	// find first matching _Val
	for (; _First != _Last; ++_First) {
		if (_First->name == _Val) {
			break;
		}
	}
	return _First;
}

bool versionMatch(std::string rver, std::string ver) {
	if (rver.empty() || ver.empty()) return false;
	switch (rver[0])
	{
	case '*':
		return true;
	case '~':
	{
		rver.erase(rver.begin()); // ɾ��~
		std::vector<std::string> v, v2;
		boost::split(v, rver, boost::is_any_of(".")); // �ָ�汾
		boost::split(v2, ver, boost::is_any_of("."));
		return !(v.size() < 2 || v2.size() < 2) && v[0] == v2[0] && v[1] == v2[1];
	}
	case '^':
	{
		rver.erase(rver.begin()); // ɾ��^
		std::vector<std::string> v, v2;
		boost::split(v, rver, boost::is_any_of(".")); // �ָ�汾
		boost::split(v2, ver, boost::is_any_of("."));
		return !(v.size() < 1 || v2.size() < 1) && v[0] == v2[0];
	}
	default:
		return false;
		break;
	}
}

