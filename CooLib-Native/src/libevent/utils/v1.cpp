#include "..\..\dll\pch.h"
#include "..\lib.h"
#include "v1.h"
#include "..\..\dll\dll.h"
#include "..\..\cq\cq.h"
#include "..\..\cpp-semver\include\cpp-semver.hpp"

namespace libutils {
	/*
		ͨ��ֱ������dll��ȡ���
	*/
	std::vector<cLibInfo> loadLib_1() {
		std::vector<cLibInfo> tlibList; // ����ֵ
		// ��ȡDLL·��
		char dllPath[MAX_PATH];
		if (GetModuleFileNameA(_hModule, dllPath, MAX_PATH) == 0) throw GetLastError();
		PathRemoveFileSpecA(dllPath);

		/* ��ȡ��� */
		char szFilePath[MAX_PATH];
		HANDLE hListFile;
		WIN32_FIND_DATA FindFileData;
		lstrcpyA(szFilePath, dllPath);
		lstrcatA(szFilePath, "\\*.*");
		hListFile = FindFirstFileA(szFilePath, &FindFileData); // �������
		if (hListFile == INVALID_HANDLE_VALUE) return tlibList; // û�в����������
		do {
			// ��������ж�
			std::string dllAppID = FindFileData.cFileName;
			dllAppID = dllAppID.substr(0, dllAppID.rfind("."));
			if (!isEnabled(dllAppID)) continue;

			// �Բ��ִ�� LoadLibrary
			HMODULE hlib;
			std::string path;
			path = path + dllPath + "\\" + FindFileData.cFileName;
			// ����������
			//auto f = std::async(PLoadLibrary, path.c_str());
			//if (f.wait_for(std::chrono::seconds(15)) != std::future_status::ready) continue; // ������볬ʱ ��ʱ��QӦ���Ѹ�������ʧ����ʾ
			//hlib = f.get();
			hlib = PLoadLibrary(path.c_str());
			if (hlib == NULL) continue; // ������Ϸ� ��ʱ��QӦ���Ѹ�������ʧ����ʾ

			// ��ȡ�����Ϣ
			typedef const char* (__stdcall* libInfoProc)();
			libInfoProc hlibInfoProc = (libInfoProc)GetProcAddress(hlib, "LibInfo");
			if (hlibInfoProc == NULL) { // ��Lib
				FreeLibrary(hlib);
				continue;
			}
			cLibInfo i(hlib, path, hlibInfoProc()); // ���ò��������
			tlibList.push_back(i); // ��������
		} while (FindNextFileA(hListFile, &FindFileData));

		return tlibList;
	}

	HMODULE PLoadLibrary(LPCSTR lpLibFileName) {
		HMODULE hlib;
		try {
			UINT ErrorMode = GetErrorMode();
			SetErrorMode(SEM_NOGPFAULTERRORBOX);
			hlib = LoadLibraryA(lpLibFileName);
			SetErrorMode(ErrorMode);
		}
		catch (...) {
			return 0;
		}
		return hlib;
	}

	/*
		�жϲ���Ƿ���
	*/
	bool isEnabled(std::string AppID) {
		// ��ȡCQ·��
		char cqPath[MAX_PATH];
		if (GetModuleFileNameA(NULL, cqPath, MAX_PATH) == 0) throw GetLastError();
		PathRemoveFileSpecA(cqPath);
		// ��ȡ���ò����
		char cqConfPath[MAX_PATH];
		lstrcpyA(cqConfPath, cqPath);
		lstrcatA(cqConfPath, "\\conf\\cqp.cfg");

		if (AppID == "cn.coorg.coolib") return true; // ��ʱ�����¼���iniд��֮ǰ

		AppID += ".status";
		return GetPrivateProfileIntA("App", AppID.c_str(), 0, cqConfPath) == 1;
	}

	namespace LibCallbackUtils {
		bool LibCallback_1(cLibInfo i, std::vector<cLibInfo> tlibList) {
			json bj = json::object();
			bj["s"] = true;

			// �������
			std::vector<std::string> bloadedLib;
			if (!i.j["require"].empty()) {
				// ����Lib�����
				for (auto libName = i.j["require"].begin(); libName != i.j["require"].end(); ++libName) {
					// �������
					auto r = isReqExist(libName.key(), libName.value().get<std::string>(), i.name, tlibList);
					if (r.empty()) {
						bloadedLib.push_back(libName.key());
					}
					else {
						cq::CQ_addLog_Error("CooLib-Native", r.c_str());
						bj["s"] = false;
					}
				}
			}

			// using���
			if (!i.j["using"].empty()) {
				// ���� using ��
				for (auto uname = i.j["using"].begin(); uname != i.j["using"].end(); ++uname) {
					try {
						bj["FuncAddr"][uname.key()] = getUsingAddr(uname.value().get<std::string>(), i.name, tlibList);
					}
					catch (std::string e) {
						cq::CQ_addLog_Error("CooLib-Native", e.c_str());
						bj["s"] = false;
					}
				}
			}
			bj["loadedLib"] = bloadedLib;

			// �������鲢��¼
			typedef int32_t(__stdcall* libCallbackProc)(bool, const char*);
			libCallbackProc hlibCallbackProc = (libCallbackProc)GetProcAddress(i.hlib, "LibCallback"); // Lib Callback
			if (hlibCallbackProc == NULL) return false;
			auto i2 = hlibCallbackProc(bj["s"].get<bool>(), bj.dump(-1, ' ', true).c_str());
			return i2 == 0;
		}

		/*
			�������
		*/
		std::string isReqExist(std::string libAppID, std::string versionRange, std::string AppID, std::vector<cLibInfo> tlibList) {
			/* ���Ҷ�Ӧ���� */
			std::vector<cLibInfo>::iterator rlib = libFind(tlibList.begin(), tlibList.end(), libAppID);

			// ����������
			if (rlib == tlibList.end() || !rlib->loaded) {
				return (boost::format("��� %s ������ %s ��ʧ�򲻴��ڣ���ȷ��ָ������Ѿ���װ�����á�") % AppID % libAppID).str();
			}
			// �����汾δƥ��
			if (!versionMatch(rlib->j["AppVer"].get<std::string>(), versionRange)) {
				return (boost::format("��� %s ������ %s �汾����ȷ��汾���Ϸ����밲װ��ȷ�İ汾��(required \"%s\" => installed \"%s\")") % AppID % libAppID % versionRange % rlib->j["AppVer"].get<std::string>()).str();
			}

			return "";
		}

		int32_t getUsingAddr(std::string usingProc, std::string AppID, std::vector<cLibInfo> tlibList) {
			std::vector<std::string> i;

			// �ָ��������
			boost::split_regex(i, usingProc, boost::regex("::"));
			// û���������������Ϸ�
			if (i.size() < 2) {
				throw (boost::format("��� %s �� json �� using �ֶε� \"%s\" ���Ϸ���") % usingProc).str();
			}

			//// ����������Ҷ�Ӧ���
			//auto clib = std::find(tlibList->begin(), tlibList->end(), i[0]);
			//// ���δ��������δ����
			//if (clib == tlibList->end()) {
			//	throw (boost::format("�յ����Բ�� %s ���������� %s ������������δ���أ�����������δ������װ�����������δ�� require �ֶ���ע�ᡣ") % AppID % usingProc).str();
			//}

			// ���������ݱ���Ҷ�Ӧ���
			std::vector<cLibInfo>::iterator rlib = libFind(tlibList.begin(), tlibList.end(), i[0]);
			if (rlib == tlibList.end() || !rlib->loaded) {
				throw (boost::format("�յ����Բ�� %s ���������� %s ������������δ���أ�����������δ������װ�����������δ�� require �ֶ���ע�ᡣ") % AppID % usingProc).str();
			}

			getUsingAddrUtils::getUsingAddr_1(i[1], rlib->j["LibAPI"], rlib->hlib);

			return (int32_t)GetProcAddress(rlib->hlib, i[1].c_str());
		}

		namespace getUsingAddrUtils {
			int32_t getUsingAddr_1(std::string usingProc, json iJson, HMODULE hlib) {
				std::vector<std::string> i;
				boost::split_regex(i, usingProc, boost::regex("::"));
				if (i.size() > 1) {
					for (auto ProcN = iJson.begin(); ProcN != iJson.end(); ++ProcN) {
						if (ProcN.value().is_object()) {
							if (ProcN.key() == i[0]) {
								int32_t i = getUsingAddr_1(usingProc, ProcN.value(), hlib);
								if (i != 0) return i;
								continue;
							}
						}
					}
				}
				else if (i.size() == 1) {
					for (auto ProcN = iJson.begin(); ProcN != iJson.end(); ++ProcN) {
						if (ProcN.value().is_string()) {
							if (ProcN.key() == usingProc) {
								return (int32_t)GetProcAddress(hlib, ProcN.value().get<std::string>().c_str());
							}
						}
					}
				}
				return 0;
			}
		}
	}

	namespace appCallbackUtils {
		int32_t appCallback_1(HMODULE hlib) {
			typedef int32_t(__stdcall* appCallbackProc)();
			appCallbackProc happCallbackProc = (appCallbackProc)GetProcAddress(hlib, "AppCallback"); // App Callback
			if (happCallbackProc == NULL) return 0;
			// �����¼�
			return happCallbackProc();
		}
	}
}