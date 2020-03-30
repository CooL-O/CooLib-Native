// ���ļ�Ϊ��ѡ���룬������ֱ�ӱ��룬�����ο�

using json = nlohmann::json;

int32_t AuthCode;
bool loaded;

typedef int32_t(__stdcall* tAdd)(int32_t a, int32_t b);
tAdd Add;

extern "C" int32_t __stdcall _eventStartup()
{
	HMODULE h;
	h = GetModuleHandleA("CQP.dll");
	CQ_addLog = reinterpret_cast<tCQ_addLog>(GetProcAddress(h, "CQ_addLog"));

	// ֪ͨ CooLib�� ��*һ��*ִ�б����������� Coolib �Ῠ�ڵȴ����ز���
	// �˲���Ӧ��������ҵ�����*��*ִ�У����ڱ�ǿ�Q���ҵ�������ִ�����
	typedef bool(__stdcall* tLibLoaded)(const char* a);
	char dllPath[MAX_PATH];
	if (GetModuleFileNameA(_hModule, dllPath, MAX_PATH) == 0) return 0;
	PathRemoveFileSpecA(dllPath);
	char szFilePath[MAX_PATH];
	lstrcpyA(szFilePath, dllPath);
	lstrcatA(szFilePath, "\\cn.coorg.coolib.cpk");
	HMODULE hcl = LoadLibraryA(szFilePath);
	if (hcl == 0) {
		lstrcpyA(szFilePath, dllPath);
		lstrcatA(szFilePath, "\\cn.coorg.coolib.dll");
		hcl = LoadLibraryA(szFilePath);
		if (hcl == 0) return 0;
	}
	tLibLoaded LibLoaded;
	DWORD l;
	LibLoaded = reinterpret_cast<tLibLoaded>(GetProcAddress(hcl, "LibLoaded"));
	if (LibLoaded) {
		LibLoaded("com.example.demo");
	}
	// �뱣�� Coolib ���أ�*��Ҫ*���� FreeLibrary

	return 0;
}

extern "C" const char* __stdcall LibInfo() { // ��������дӦ�������Ϣ������libinfo.sample.json����ע�ⲻҪ����jsonע��
	return (const char*)R"(
{
    "ver": 1,
    "AppID": "com.example.demo",
    "AppVer": "1.0.0",
    "require": {
        "cn.coorg.coolib.testlib": "*"
    },
    "using": {
        "Add": "cn.coorg.coolib.testlib::Add"
    },
    "LibAPI": {
		"ExampleAddFunc": "ExampleAddFunc",
		"Dec": {
			"ExampleDecFunc": "ExampleDecFunc"
		}
	}
}
)";
}

extern "C" int32_t __stdcall LibCallback(bool success, const char* callbackJson) { // �������¼�
	if (success) {
		json j = json::parse(callbackJson);
		Add = reinterpret_cast<tAdd>(j["FuncAddr"]["Add"].get<int32_t>());
		// TODO: PUT YOUR CODE HERE
	}
	return 0;
}

extern "C" int32_t __stdcall AppCallback() { // Ӧ�������¼�
	// TODO: PUT YOUR CODE HERE
	return 0;
}

extern "C" int32_t __stdcall DisableCallback() { // Ӧ��ж���¼�
	// TODO: PUT YOUR CODE HERE
	return 0;
}

extern "C" int32_t __stdcall ExitCallback(const char* callbackJson) { // ��ж���¼�
	// TODO: PUT YOUR CODE HERE
	FreeLibrary(examplelib);
	return 0;
}

extern "C" int32_t __stdcall ExampleAddFunc(int32_t a, int32_t b) { // һ��ʾ������
	return a + b;
}
