// ���ļ�Ϊ��ѡ���룬������ֱ�ӱ��룬�����ο�

using json = nlohmann::json;

HMODULE examplelib;

extern "C" const char* __stdcall LibInfo() { // ��������дӦ�������Ϣ������libinfo.sample.json����ע�ⲻҪ����jsonע��
	return (const char*)R"(
{
    "ver": 1,
    "AppID": "com.example.demo",
    "AppVer": "1.0.0",
    "lib": [
        {
            "LibAppID": "cn.coorg.coolib",
            "LibVer": "*"
        }
    ],
    "LibAPI": [
        {
            "func": "ExampleAddFunc",
            "?": "@@YGHHH@Z"
        }
    ]
}
)";
}

extern "C" int32_t __stdcall LibCallback(bool success, const char* callbackJson) { // �������¼����������ʼ������API�ӿ�
	if (success) {
		json j = json::parse(callbackJson);
		examplelib = LoadLibraryA(callbackJson["libPath"]["cn.coorg.coolib"].get<std::string>().c_str());
		// TODO: PUT YOUR CODE HERE
	}
	return 0;
}

extern "C" int32_t __stdcall AppCallback() { // Ӧ�������¼����������ʼ��Ӧ��
	return 0;
}

extern "C" int32_t __stdcall DisableCallback() { // Ӧ��ж���¼������������Ӧ��
	return 0;
}

extern "C" int32_t __stdcall ExitCallback(const char* callbackJson) { // ��ж���¼��������������
	// TODO: PUT YOUR CODE HERE
	FreeLibrary(examplelib);
	return 0;
}

extern "C" int32_t __stdcall ExampleAddFunc(int32_t a, int32_t b) { // һ��ʾ������
	return a + b;
}
