// cqevent.cpp ����CQ�¼�
#include "pch.h"

#include "dll.h"
#include "cqevent.h"
#include "cqapi.h"

using json = nlohmann::json;

std::vector<LibInfo> libList; // �����

const char* AppInfo()
{
	return "9,cn.coorg.coolib";
}

int32_t Initialize(int32_t AuthCode)
{
	cq::AuthCode = AuthCode;
	cq::__init_api();
	return 0;
}

int32_t _eventStartup()
{
	// ��ȡDLL·��
	char dllPath[MAX_PATH];
	GetModuleFileNameA(_hModule, dllPath, MAX_PATH);
	PathRemoveFileSpecA(dllPath);

	std::vector<LibInfo> tlibList; // ��ʱ�����

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
			if (hlib != NULL) {
				typedef const char* (__stdcall* libInfoProc)();
				libInfoProc hlibInfoProc = (libInfoProc)GetProcAddress(hlib, "LibInfo"); // ��ȡLib��Ϣ
				if (hlibInfoProc != NULL) {
					json j = json::parse(hlibInfoProc());
					LibInfo i(hlib, path, j);
					tlibList.push_back(i);
				}
			}
		} while (FindNextFileA(hListFile, &FindFileData));
	}

	// ������
	std::sort(tlibList.begin(), tlibList.end(), upsort);

	// ��������
	for (LibInfo i : tlibList) {
		if (i.j["ver"] == 1) {
			json bj = json::parse("{\"s\":true}");
			std::vector<std::string> bmissLib;
			std::map<std::string, std::string> libPath;
			for (std::string libName : i.j["lib"]) { // ����Lib�����
				if (!libName.empty()) {
					std::vector<LibInfo>::iterator rlib = libFind(tlibList.begin(), tlibList.end(), libName);
					if (rlib != tlibList.end()) { // ����
						if (rlib->loaded) { // �Ѽ���
							libPath[rlib->name] = rlib->path;
						}
						else { // δ����
							std::string errorInfo;
							errorInfo = errorInfo + "��� " + i.name + " ������ " + libName + " δ��ȷ���أ���ȷ�ϲ�����ȼ�������ȷ��priority�ֶΣ���";
							cq::CQ_addLog_Error("CooLib-Native", errorInfo.c_str());
							bj["s"] = false;
							bmissLib.push_back(libName);
						}
					}
					else { // ������
						std::string errorInfo;
						errorInfo = errorInfo + "��� " + i.name + " ������ " + libName + " ��ʧ�򲻴��ڣ���ȷ��ָ������Ѿ���װ�����á�";
						cq::CQ_addLog_Error("CooLib-Native", errorInfo.c_str());
						bj["s"] = false;
						bmissLib.push_back(libName);
					}
				}
			}
			if (!bmissLib.empty()) bj["missLib"] = bmissLib;
			bj["libPath"] = libPath;
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
	for (LibInfo i : tlibList) {
		if (i.loaded) {
			libList.push_back(i);
		}
		else {
			FreeLibrary(i.hlib); // �ͷ��ļ�
		}
	}

	return 0;
}

int32_t _eventEnable() {

	// ֪ͨ�¼�
	for (LibInfo i : libList) {
		typedef int32_t(__stdcall* appCallbackProc)(bool, const char*);
		appCallbackProc happCallbackProc = (appCallbackProc)GetProcAddress(i.hlib, "AppCallback"); // App Callback
		if (happCallbackProc != NULL) {
			appCallbackProc(); // �����¼�
		}
	}

	return 0;
}

int32_t _eventDisable() {

	// ��������
	std::sort(libList.begin(), libList.end(), downsort);

	// ֪ͨ�¼�
	for (LibInfo i : libList) {
		typedef int32_t(__stdcall* disableCallbackProc)(bool, const char*);
		disableCallbackProc hdisableCallbackProc = (disableCallbackProc)GetProcAddress(i.hlib, "DisableCallback"); // App Callback
		if (hdisableCallbackProc != NULL) {
			disableCallbackProc(); // �����¼�
		}
	}

	return 0;
}

int32_t _eventExit()
{
	// ֪ͨ�¼�
	for (LibInfo i : libList) {
		std::vector<std::string> bloadLib;
		for (std::string libName : i.j["lib"]) { // ����Lib�����
			if (!libName.empty()) {
				std::vector<LibInfo>::iterator rlib = libFind(libList.begin(), libList.end(), libName);
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
		typedef int32_t(__stdcall* disableCallbackProc)(const char*);
		disableCallbackProc hdisableCallbackProc = (disableCallbackProc)GetProcAddress(i.hlib, "DisableCallback"); // App Callback
		if (hdisableCallbackProc != NULL) {
			disableCallbackProc(bj.dump().c_str()); // �����¼�
		}
	}

	for (LibInfo i : libList) {
		FreeLibrary(i.hlib); // �ͷ��ļ�
	}

	return 0;
}

bool upsort(LibInfo i, LibInfo j) {
	return i.priority < j.priority;
}

bool downsort(LibInfo i, LibInfo j) {
	return i.priority > j.priority;
}

std::vector<LibInfo>::iterator libFind(std::vector<LibInfo>::iterator _First, const std::vector<LibInfo>::iterator _Last, const std::string& _Val) {
	// find first matching _Val
	for (; _First != _Last; ++_First) {
		if (_First->name == _Val) {
			break;
		}
	}
	return _First;
}
