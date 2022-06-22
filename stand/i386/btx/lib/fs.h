

typedef int btx_ino_t;

struct btx_file_provider {
	btx_ino_t(*open)(const char*);
	int(*read)(btx_ino_t, void*, int);
	void(*close)(btx_ino_t);
};

extern struct btx_file_provider provider;