#include "..\dll\pch.h"
#include "lib.h"
#include "utils\v1.h"
#include "..\cq\cq.h"
#include "..\cpp-semver\include\cpp-semver.hpp"

using json = nlohmann::json;

namespace libutils {
	std::vector<cLibInfo> libList; // �����
	std::queue<std::string> loadedAppIDList; // �Ѿ���ȡ AuthCode �� Lib ��
	HANDLE hCQThread;
	HANDLE hCQThreadEvent;
	HANDLE hCQThreadExitEvent;

	DWORD WINAPI CQThreadProc(LPVOID lpParameter) {
		/* ��ʼ�� */
		std::vector<cLibInfo> tlibListTmp;
		try {
			tlibListTmp = loadLib_1();
		}
		catch (DWORD e) {
			cq::CQ_addLog_Info("Loader", (
				boost::format("LoadDLL failed (system) with error code: %d") % e
				).str().c_str());
		}

		hCQThreadEvent = CreateSemaphoreA(NULL, 0, tlibListTmp.size(), NULL);

		cq::CQ_addLog_Debug("Loader", (
			boost::format("Working apps: %d") % tlibListTmp.size()
			).str().c_str());

		std::vector<cLibInfo> tlibList;
		while (!loadedAppIDList.empty()) {
			std::string i = loadedAppIDList.front();
			std::vector<cLibInfo>::iterator rlib = libFind(tlibListTmp.begin(), tlibListTmp.end(), i);
			if (rlib != tlibListTmp.end()) tlibList.push_back(*rlib);
			cq::CQ_addLog_Debug("Loader-Pusher", (
				boost::format("Pushed app: %s") % rlib->name
				).str().c_str());
			loadedAppIDList.pop();
		}

		while (tlibList.size() != tlibListTmp.size()) {
			if (hCQThreadEvent != 0) {
				DWORD dwEvent;
				if (hCQThreadExitEvent) {
					HANDLE handleArray[2] = { hCQThreadEvent , hCQThreadExitEvent };
					dwEvent = WaitForMultipleObjects(2, handleArray, false, INFINITE);
				}
				else {
					WaitForSingleObject(hCQThreadEvent, INFINITE);
					dwEvent = WAIT_OBJECT_0 + 0;
				}

				switch (dwEvent)
				{
				case WAIT_OBJECT_0 + 0:
					while (!loadedAppIDList.empty()) {
						std::string i = loadedAppIDList.front();
						std::vector<cLibInfo>::iterator rlib = libFind(tlibListTmp.begin(), tlibListTmp.end(), i);
						if (rlib != tlibListTmp.end()) tlibList.push_back(*rlib);
						cq::CQ_addLog_Debug("Loader-Pusher", (
							boost::format("Pushed app: %s") % rlib->name
							).str().c_str());
						loadedAppIDList.pop();
					}
					break;
				case WAIT_OBJECT_0 + 1:
				case WAIT_TIMEOUT:
				default:
					CloseHandle(hCQThreadEvent);
					hCQThreadEvent = 0;
					CloseHandle(hCQThreadExitEvent);
					hCQThreadExitEvent = 0;
					return 0;
				}
			}
			else return 0;
		}

		cq::CQ_addLog_Debug("Loader", "Calling apps.");

		LARGE_INTEGER time;
		QueryPerformanceFrequency(&time);
		LONGLONG timeDff = time.QuadPart;
		QueryPerformanceCounter(&time);
		__int64 timeStart = time.QuadPart;

		loadLib(tlibList);

		QueryPerformanceCounter(&time);
		__int64 timeEnd = time.QuadPart;
		cq::CQ_addLog_Info("Loader", (
			boost::format("CooLib loaded.(%d total, %d succeed, %fms)") % tlibList.size() % libList.size() % ((double)(timeEnd - timeStart) * 1000 / timeDff)
			).str().c_str());
		return 0;
	}
	/*
		���� Lib
	*/
	void loadLib(std::vector<cLibInfo> _tlibList) {
		/* ����ǰ��֤ */
		if (!libList.empty()) unloadLib();

		auto tlibList = _tlibList;

		/* ������ */
		tlibList = LibSort(tlibList);
		if (tlibList.empty()) {
			std::string emsg = "Lib�б�Ϊ�գ����ܲ��������ѭ��������������ų������\r\n����б�\r\n";
			for (auto i : _tlibList) {
				emsg += i.name + "\r\n";
			}
			cq::CQ_addLog_Error("Loader", emsg.c_str());
		}

		cq::CQ_addLog_Debug("Loader", "LibCallback:");

		/* ������ */
		LibCallback(&tlibList);

		/* ����ȫ����Ϣ�� */
		for (cLibInfo& i : tlibList) {
			if (i.loaded) {
				libList.push_back(i);
			}
			else {
				FreeLibrary(i.hlib); // �ͷ��ļ�
			}
		}

		cq::CQ_addLog_Debug("Loader", "AppCallback:");

		/* ֪ͨ�¼� */
		appCallback(&tlibList);

		return;
	}

	/*
		��Ӧ��ִ�� LibCallback
	*/
	void LibCallback(std::vector<cLibInfo>* tlibList) {
		for (cLibInfo& i : *tlibList) {
			if (i.j["ver"] == 1) {
				cq::CQ_addLog_Debug("Loader-LibCallback", (
					boost::format("Calling %s:") % i.name
					).str().c_str());

				LARGE_INTEGER time;
				QueryPerformanceFrequency(&time);
				LONGLONG timeDff = time.QuadPart;
				QueryPerformanceCounter(&time);
				__int64 timeStart = time.QuadPart;

				i.loaded = LibCallbackUtils::LibCallback_1(i, *tlibList);

				QueryPerformanceCounter(&time);
				__int64 timeEnd = time.QuadPart;
				cq::CQ_addLog_Debug("Loader-LibCallback", (
					boost::format("Called %s.(%fms)") % i.name % ((double)(timeEnd - timeStart) * 1000 / timeDff)
					).str().c_str());
			}
		}
	}

	/*
		��Ӧ��ִ�� appCallback
	*/
	void appCallback(std::vector<cLibInfo>* tlibList) {
		for (cLibInfo& i : *tlibList) {
			if (i.j["ver"] == 1) {
				cq::CQ_addLog_Debug("Loader-AppCallback", (
					boost::format("Calling %s:") % i.name
					).str().c_str());

				LARGE_INTEGER time;
				QueryPerformanceFrequency(&time);
				LONGLONG timeDff = time.QuadPart;
				QueryPerformanceCounter(&time);
				__int64 timeStart = time.QuadPart;

				i.appLoaded = appCallbackUtils::appCallback_1(i.hlib) == 0;

				QueryPerformanceCounter(&time);
				__int64 timeEnd = time.QuadPart;
				cq::CQ_addLog_Debug("Loader-AppCallback", (
					boost::format("Called %s.(%fms)") % i.name % ((double)(timeEnd - timeStart) * 1000 / timeDff)
					).str().c_str());
			}
		}
	}

	void unloadLib() { // ж��Lib
		/* ж��ǰ��֤�����ⲻ��Ҫ���ж� */
		if (libList.empty()) return;

		LARGE_INTEGER time;
		QueryPerformanceFrequency(&time);
		LONGLONG timeDff = time.QuadPart;
		QueryPerformanceCounter(&time);
		__int64 timeStart = time.QuadPart;

		auto itotal = libList.size();

		/* �������� */
		libList = LibSort(libList);
		std::reverse(libList.begin(), libList.end());

		/* ֪ͨ�¼� */
		disableCallback(&libList);

		/* ж�� */
		exitCallback(&libList);

		/* ��ձ� */
		libList.clear();

		QueryPerformanceCounter(&time);
		__int64 timeEnd = time.QuadPart;
		cq::CQ_addLog_Info("Loader", (
			boost::format("CooLib unloaded.(%d total, %fms)") % itotal % ((double)(timeEnd - timeStart) * 1000 / timeDff)
			).str().c_str());

		return;
	}

	void disableCallback(std::vector<cLibInfo>* tlibList) {
		for (cLibInfo& i : *tlibList) {
			if (i.j["ver"] == 1) {
				cq::CQ_addLog_Debug("Loader-DisableCallback", (
					boost::format("Calling %s:") % i.name
					).str().c_str());

				LARGE_INTEGER time;
				QueryPerformanceFrequency(&time);
				LONGLONG timeDff = time.QuadPart;
				QueryPerformanceCounter(&time);
				__int64 timeStart = time.QuadPart;

				i.appLoaded = disableCallbackUtils::disableCallback_1(i.hlib) != 0;

				QueryPerformanceCounter(&time);
				__int64 timeEnd = time.QuadPart;
				cq::CQ_addLog_Debug("Loader-DisableCallback", (
					boost::format("Called %s.(%fms)") % i.name % ((double)(timeEnd - timeStart) * 1000 / timeDff)
					).str().c_str());
			}
		}
	}

	void exitCallback(std::vector<cLibInfo>* tlibList) {
		for (cLibInfo& i : *tlibList) {
			if (i.j["ver"] == 1) {
				cq::CQ_addLog_Debug("Loader-ExitCallback", (
					boost::format("Calling %s:") % i.name
					).str().c_str());

				LARGE_INTEGER time;
				QueryPerformanceFrequency(&time);
				LONGLONG timeDff = time.QuadPart;
				QueryPerformanceCounter(&time);
				__int64 timeStart = time.QuadPart;

				i.loaded = !exitCallbackUtils::exitCallback_1(i, *tlibList);

				QueryPerformanceCounter(&time);
				__int64 timeEnd = time.QuadPart;
				cq::CQ_addLog_Debug("Loader-ExitCallback", (
					boost::format("Called %s.(%fms)") % i.name % ((double)(timeEnd - timeStart) * 1000 / timeDff)
					).str().c_str());
			}
		}
	}

	//void reloadLib() { // ���ز��
	//	if (!libList.empty()) {
	//		unloadLib();
	//	}
	//	loadLib();
	//	return;
	//}

	std::vector<cLibInfo> LibSort(const std::vector<cLibInfo> sortLib) { // ���亯��
		std::map<std::string, int> indegree;
		std::map<std::string, std::vector<std::string>> adj;
		for (const cLibInfo& i : sortLib) {
			indegree[i.name] = 0;
			if (!i.j["require"].empty()) {
				for (auto libName = i.j["require"].begin(); libName != i.j["require"].end(); ++libName) {
					adj[libName.key()].push_back(i.name);
					++indegree[i.name];
				}
			}
		}
		std::vector<std::string> sortedT = TSort(indegree, adj);
		std::vector<cLibInfo> rl;
		for (std::string i : sortedT) {
			for (const cLibInfo& i2 : sortLib) {
				if (i2.name == i) {
					rl.push_back(i2);
				}
			}
		}
		return rl;
	}

	std::vector<std::string> TSort(const std::map<std::string, int> _indegree, const std::map<std::string, std::vector<std::string>> _adj) { // ��������
		std::map<std::string, int> indegree = _indegree;
		std::map<std::string, std::vector<std::string>> adj = _adj;
		std::queue<std::string> q;
		for (auto iter = indegree.begin(); iter != indegree.end(); iter++) { // �սڵ����
			if (indegree[iter->first] == 0) {
				q.push(iter->first);
			}
		}

		std::vector<std::string> rv;
		size_t count = 0;
		while (!q.empty())
		{
			// ȡ��һ������
			std::string v = q.front();
			q.pop();

			// ����õ�
			rv.push_back(v);

			++count;

			for (auto iter : adj[v]) { // �սڵ����
				if (--indegree[iter] == 0) {
					q.push(iter);
				}
			}
		}

		if (count < adj.size()) {
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

}

cLibInfo::cLibInfo(HMODULE _hlib, std::string _path, std::string _j) {
	hlib = _hlib;
	path = _path;

	std::regex e("/\\*[\\s\\S]*\\*/|//.*");
	std::string uj = std::regex_replace(_j, e, "");
	j = json::parse(uj);
	name = j["AppID"].get<std::string>();
	loaded = false;
}
