#ifndef LIBDRAWTEXT_H
#define LIBDRAWTEXT_H

#ifdef __cplusplus
extern "C"
{
#endif

	int Configure(void **ctxp, int argc, char *argv[]);
	void Process(void *ctx, void *data,int width, int height, long long  pts);
	void Release(void *ctx);

#ifdef __cplusplus
};
#endif



#endif