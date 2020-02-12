#include "..\dll\pch.h"
#include "lib.h"
#include "..\dll\dll.h"
#include "..\cqapi\cq.h"
#include "..\cpp-semver\include\cpp-semver.hpp"

using json = nlohmann::json;

std::vector<cLibInfo> libList; // �����

void loadLib() { // ����Lib
	if (!libList.empty()) unloadLib();
	// ��ȡDLL·��
	char dllPath[MAX_PATH];
	GetModuleFileNameA(_hModule, dllPath, MAX_PATH);
	PathRemoveFileSpecA(dllPath);
	// ��ȡCQ·��
	char cqPath[MAX_PATH];
	GetModuleFileNameA(NULL, cqPath, MAX_PATH);
	PathRemoveFileSpecA(cqPath);

	std::vector<cLibInfo> tlibList; // ��ʱ�����

	// ��ȡ���ò����
	char cqpConfPath[MAX_PATH];
	lstrcpyA(cqpConfPath, cqPath);
	lstrcatA(cqpConfPath, "\\conf\\cqp.cfg");

	// ����������
	char szFilePath[MAX_PATH];
	HANDLE hListFile;
	WIN32_FIND_DATA FindFileData;
	lstrcpyA(szFilePath, dllPath);
	lstrcatA(szFilePath, "\\*.*");
	hListFile = FindFirstFileA(szFilePath, &FindFileData); // �������
	if (hListFile != INVALID_HANDLE_VALUE) {
		do {
			// �жϲ���Ƿ�����
			std::string dllAppID = FindFileData.cFileName;
			dllAppID = dllAppID.substr(0, dllAppID.rfind("."));
			if (dllAppID != "cn.coorg.coolib") { // �е�ʱ�������¼���iniд��֮ǰ
				dllAppID += ".status";
				if (GetPrivateProfileIntA("App", dllAppID.c_str(), 0, cqpConfPath) != 1) continue;
			}
			// ������
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
			cLibInfo i(hlib, path, hlibInfoProc());
			tlibList.push_back(i);
		} while (FindNextFileA(hListFile, &FindFileData));
	}

	// ������
	libList = LibSort(libList);

	// ��������
	for (cLibInfo& i : tlibList) {
		if (i.j["ver"] == 1) {
			json bj = json::object();
			bj["s"] = true;
			std::vector<std::string> bmissLib;
			if (!i.j["lib"].empty()) {
				for (auto libName = i.j["require"].begin(); libName != i.j["require"].end(); ++libName) { // ����Lib�����
					std::vector<cLibInfo>::iterator rlib = libFind(tlibList.begin(), tlibList.end(), libName.key());
					if (rlib == tlibList.end()) { // ������
						cq::CQ_addLog_Error("CooLib-Native", (
							boost::format("��� %s ������ %s ��ʧ�򲻴��ڣ���ȷ��ָ������Ѿ���װ�����á�") % i.name % libName.key()
							).str().c_str());
						bj["s"] = false;
						bmissLib.push_back(libName.key());
						continue;
					}
					if (!rlib->loaded) { // δ����
						cq::CQ_addLog_Error("CooLib-Native", (
							boost::format("��� %s ������ %s δ��ȷ���ء�") % i.name % libName.key()
							).str().c_str());
						bj["s"] = false;
						bmissLib.push_back(libName.key());
						continue;
					}
					if (!versionMatch(rlib->j["AppVer"].get<std::string>(), i.j["require"][libName.key()].get<std::string>())) { // �汾δƥ��
						cq::CQ_addLog_Error("CooLib-Native", (
							boost::format("��� %s ������ %s �汾����ȷ��汾���Ϸ����밲װ��ȷ�İ汾��(required \"%s\" => installed \"%s\")") % i.name % libName.key() % i.j["require"][libName.key()].get<std::string>() % rlib->j["AppVer"].get<std::string>()
							).str().c_str());
						bj["s"] = false;
						bmissLib.push_back(libName.key());
						continue;
					}
				}
			}
			if (!i.j["using"].empty()) {
				for (auto uname = i.j["require"].begin(); uname != i.j["require"].end(); ++uname) { // ����Lib�����
					std::vector<std::string> i2;
					boost::split(i2, i.j["using"][uname.key()].get<std::string>(), boost::is_any_of("::"));
					if (i2.size() != 2) {
						cq::CQ_addLog_Error("CooLib-Native", (
							boost::format("��� %s �� json �� using �ֶε� \"%s\" ���Ϸ���") % i.name % i.j["using"][uname.key()].get<std::string>()
							).str().c_str());
						bj["s"] = false;
						continue;
					}
					std::vector<cLibInfo>::iterator rlib = libFind(tlibList.begin(), tlibList.end(), i2[0]);
					for (std::string i3 : bmissLib) { // lib missing
						if (rlib->name == i3) {
							goto nextU;
						}
					}
					if (rlib == tlibList.end()) { // ������
						cq::CQ_addLog_Error("CooLib-Native", (
							boost::format("�յ����Բ�� %s ���������� %s ������������δ�� require �ֶ���ע�ᡣ") % i.name % uname.key()
							).str().c_str());
						bj["s"] = false;
						bmissLib.push_back(uname.key());
						continue;
					}
					bj["FuncAddr"][uname.key()] = (int32_t)GetProcAddress(rlib->hlib, i2[1].c_str());
				nextU:;
				}
			}
			bj["missLib"] = bmissLib;
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
	for (cLibInfo& i : tlibList) {
		if (i.loaded) {
			libList.push_back(i);
		}
		else {
			FreeLibrary(i.hlib); // �ͷ��ļ�
		}
	}

	// ֪ͨ�¼�
	for (cLibInfo& i : libList) {
		typedef int32_t(__stdcall* appCallbackProc)();
		appCallbackProc happCallbackProc = (appCallbackProc)GetProcAddress(i.hlib, "AppCallback"); // App Callback
		if (happCallbackProc != NULL) {
			appCallbackProc(); // �����¼�
		}
	}

	return;
}

void unloadLib() { // ж��Lib
	if (libList.empty()) return;

	// ��������
	std::reverse(libList.begin(), libList.end());

	// ֪ͨ�¼�
	for (cLibInfo& i : libList) {
		typedef int32_t(__stdcall* disableCallbackProc)();
		disableCallbackProc hdisableCallbackProc = (disableCallbackProc)GetProcAddress(i.hlib, "DisableCallback"); // App Callback
		if (hdisableCallbackProc != NULL) {
			disableCallbackProc(); // �����¼�
		}
	}

	// ж��
	for (cLibInfo& i : libList) {
		std::vector<std::string> bloadLib;
		json bj = json::object();
		if (!i.j["lib"].empty()) {
			for (auto libName = i.j["require"].begin(); libName != i.j["require"].end(); ++libName) { // ����Lib�����
				std::vector<cLibInfo>::iterator rlib = libFind(libList.begin(), libList.end(), libName.key());
				if (rlib->loaded) { // �Ѽ���
					bloadLib.push_back(rlib->name);
				}
				else { // δ����
					cq::CQ_addLog_Error("CooLib-Native", (
						boost::format("��� %s ������ %s δ��ȷ���ء�") % i.name % libName.key()
						).str().c_str());
				}
			}
			bj["loadLib"] = bloadLib;
		}
		typedef int32_t(__stdcall* exitCallbackProc)(const char*);
		exitCallbackProc hexitCallbackProc = (exitCallbackProc)GetProcAddress(i.hlib, "ExitCallback"); // App Callback
		if (hexitCallbackProc != NULL) {
			exitCallbackProc(bj.dump().c_str()); // �����¼�
		}
		i.loaded = false;
		FreeLibrary(i.hlib); // �ͷŲ��
	}
	libList.clear(); // ��ձ�
	return;
}

void reloadLib() { // ���ز��
	if (!libList.empty()) {
		unloadLib();
	}
	loadLib();
	return;
}

std::vector<cLibInfo> LibSort(const std::vector<cLibInfo> sortLib) { // ���亯��
	std::vector<cTopological> sortT;
	for (const cLibInfo& i : sortLib) {
		cTopological _i;
		_i.name = i.name;
		if (!i.j["require"].empty()) {
			for (auto libName = i.j["require"].begin(); libName != i.j["require"].end(); ++libName) {
				_i.libNames.push_back(libName.key());
			}
		}
		sortT.push_back(_i);
	}
	std::vector<std::string> sortedT = TSort(sortT);
	std::vector<cLibInfo> rl;
	for (std::string i : sortedT) {
		for (const cLibInfo& i2 : sortLib) {
			if (i2.name == i) {
				rl.push_back(i2);
				continue;
			}
		}
	}
	return rl;
}

std::vector<std::string> TSort(const std::vector<cTopological> sortT) { // ��������
	std::vector<cTopological> sortT2 = sortT;
	std::queue<std::string> q;
	for (cTopological i : sortT2) { // �սڵ����
		if (i.libNames.empty()) {
			q.push(i.name);
		}
	}
	std::vector<std::string> rv;
	size_t count = 0;
	while (!q.empty())
	{
		std::string v = q.front(); // ȡ��һ������
		q.pop();
		rv.push_back(v);
		++count;
		// ɾ������
		for (cTopological& i : sortT2) {
			for (auto i2 = i.libNames.begin(); i2 != i.libNames.end(); ) {
				if (*i2 == v) {
					i2 = i.libNames.erase(i2);
				}
				else {
					i2++;
				}
			}
		}
		// �սڵ����
		for (cTopological i : sortT2) {
			if (i.libNames.empty()) {
				q.push(i.name);
			}
		}
	}
	if (count < sortT.size()) {
		return std::vector<std::string>();
	}
	else {
		return rv;
	}
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

bool versionMatch(std::string version, std::string range) {
	return semver::satisfies(version, range);
}
