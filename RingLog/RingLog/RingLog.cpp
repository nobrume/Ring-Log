// RingLog.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "rlog.h"
#include <pthread.h>

void* thdo(void* args)
{
	int i;
	for (i = 0; i < 1e7; ++i)
	{
		LOG_ERROR("my number is %d", i);
	}
	printf("end i = %d", i);
	return NULL;
}

int _tmain(int argc, _TCHAR* argv[])
{

	LOG_INIT("log", "sea", 3);
	pthread_t tids[5];
	for (int i = 0; i < 5; ++i)
 		pthread_create(&tids[i], NULL, thdo, NULL);

	for (int i = 0; i < 5; ++i)
		pthread_join(tids[i], NULL);
	//need some check ensure the buffer save to file
	LOG_DETACH();

	system("pause");
	return 0;
}

// int64_t get_current_millis(void) {
// 	struct timeval tv;
// 	gettimeofday(&tv, NULL);
// 	return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
// }



