#include <IOKit/IOKitLib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct BufferDesc {
    void* start;
    size_t length;
};

#define NUM_BUFFERS_TO_STORE 15

static struct BufferDesc gBufferDescs[NUM_BUFFERS_TO_STORE] = {};
static size_t gBuffersStored;

static void dumpGPUData(void)
{
    fprintf(stderr, "%s: DUMPING GPU BUFFERS total=%zu\n",
			__func__, gBuffersStored);
    for (size_t i = 0; i < gBuffersStored; i++) {
        struct BufferDesc desc = gBufferDescs[i];

        char fname[256] = {};
        fprintf(stderr, "GPU_%ld_%p_%zx", time(NULL), desc.start, desc.length);
        sprintf(fname, "GPU_%ld_%p_%zx", time(NULL), desc.start, desc.length);
        FILE* fout = NULL;
        fout = fopen(fname, "wb");
        if (!fout) {
            fprintf(stderr, "failed to open %s\n", fname);
            continue;
        }
        fwrite(desc.start, desc.length, 1, fout);
        fclose(fout);
    }
}

static void corruptGPUData(void)
{
    static int count = 0;
    count++;
    if (count < 10 || count > 14) {
        //avoid locking up the machine for too long
		srand(time(NULL));
        return;
    }

#if 0
	dumpGPUData();
	return;
#endif

    fprintf(stderr, "%s: CORRUPTING GPU BUFFERS total=%zu\n",
			__func__, gBuffersStored);
    for (size_t i = 0; i < gBuffersStored; i++) {
        struct BufferDesc desc = gBufferDescs[i];
        if (desc.length > 0x100000) {
            continue;
        }

        for (size_t j = 0; j < 128; j++) {
			size_t max = (0x1900 / sizeof(int));
			size_t idx = rand() % max;
            int fuzz = 1 << (rand() % 24);
            ((int*)desc.start)[idx] ^= fuzz;
        }
    }
}

extern void* IOAccelResourceCreate(
    void* io_accelerator,
    void* args,
    size_t size);

extern void* IOAccelResourceGetDataBytes(void* io_accel_resource);
extern size_t IOAccelResourceGetDataSize(void* io_accel_resource);

static void fhd(FILE* f, const char* label, unsigned char* ptr, size_t len)
{
    if (!f) {
        return;
    }
    if (!ptr) {
        return;
    }
    if (!len) {
        return;
    }

    fprintf(f, "%s\n", label);
    for (size_t i = 0; i < len; i++) {
        fprintf(f, "%02x ", ptr[i]);
        if (i && ((i % 32) == 0)) {
            fputs("\n", f);
        }
    }
    fputs("\n", f);
}

extern void* fake_IOAccelResourceCreate(
    void* io_accelerator,
    void* args,
    size_t size)
{
    void* ret = IOAccelResourceCreate(io_accelerator, args, size);

    if (ret) {
        struct BufferDesc desc = {};

        if (1) {
            size_t* ptr = (size_t*)(((size_t)ret) + 0x20);
            desc.start = (void*)ptr[0];
            desc.length = ptr[1];
        } else {
            desc.start = IOAccelResourceGetDataBytes(ret);
            desc.length = IOAccelResourceGetDataSize(ret);
        }
		if (!desc.start || !desc.length)
		{
			return ret;
		}
        fprintf(stderr, "%s: registered rsrc base %p size %zx\n",
            __func__, desc.start, desc.length);

        if (gBuffersStored < NUM_BUFFERS_TO_STORE) {
            gBufferDescs[gBuffersStored] = desc;
            gBuffersStored++;
        }
    }

    return ret;
}

//these are used on iOS and M1 Macs
typedef void* my_IOGPU_Resource;
extern void * IOGPUResourceGetDataBytes(
		my_IOGPU_Resource);
extern size_t IOGPUResourceGetGPUVirtualAddress(
		my_IOGPU_Resource);
extern size_t IOGPUResourceGetDataSize(
		my_IOGPU_Resource);
extern my_IOGPU_Resource IOGPUResourceCreate(
		void *arg1,
		void *arg2,
		size_t arg3);

static my_IOGPU_Resource fake_IOGPUResourceCreate(
		void *arg1,
		void *arg2,
		size_t arg3)
{
	fprintf(stderr, "%s: arg1=%p arg2=%p arg3=%zx\n",
			__func__, arg1, arg2, arg3);
	void *ret = IOGPUResourceCreate(arg1, arg2, arg3);
	if (!ret) {
		return NULL;
	}
    size_t gpu_va = IOGPUResourceGetGPUVirtualAddress(ret);
	size_t gpu_size = IOGPUResourceGetDataSize(ret);
    void *gpu_data = IOGPUResourceGetDataBytes(ret);
    fprintf(stderr, "%s: ret=%p gpu_va=%zx gpu_data=%p size=%zx\n",
        __func__, ret, gpu_va, gpu_data, gpu_size);

	if (!gpu_data || !gpu_size) {
		return ret;
	}
	if (gBuffersStored < NUM_BUFFERS_TO_STORE) {
		gBufferDescs[gBuffersStored] = (struct BufferDesc) {
			.start = gpu_data,
			.length = gpu_size,
		};
		gBuffersStored++;
	}
	return ret;
}

extern kern_return_t fake_IOConnectCallMethod(mach_port_t connection, // rdi
    uint32_t selector, // rsi
    uint64_t* input, // rdx
    uint32_t inputCnt, // rcx
    void* inputStruct, // r8
    size_t inputStructCnt, // r9
    uint64_t* output,
    uint32_t* outputCnt,
    void* outputStruct,
    size_t* outputStructCntP)
{
    kern_return_t ret;

    if (0) {
        fprintf(stderr,
            "%s: connection=%x, selector=%x, input=%p"
            ", inputCnt=%d, inputStruct=%p, inputStructCnt=%lx"
            ", output=%p outputCnt=%x outputStruct=%p outputStructCntP=%p\n",
            __func__, connection, selector, input, inputCnt, inputStruct,
            inputStructCnt, output, outputCnt ? *outputCnt : 0, outputStruct,
            outputStructCntP);
    }

	switch (selector) {
		//case 0xd:
		//case 0x2c:
		//case 0x20:
		//case 0x8:
		//case 0x11:
		//case 0x1d:
		//case 0x17:
		//case 0xf:
		//case 0x1b:
		case 0x12:
		case 0x1e:
		case 0:
		case 9:
		case 0xa:
		case 0xb:
			//this is SubmitCommandBuffers
			corruptGPUData();
		default:
			break;
    }

    ret = IOConnectCallMethod(
        connection, selector, input, inputCnt,
        inputStruct, inputStructCnt, output,
        outputCnt, outputStruct, outputStructCntP);

    return ret;
}

typedef struct interposer {
    void* replacement;
    void* original;
} interpose_t;

__attribute__((used)) static const interpose_t interposers[]
    __attribute__((section("__DATA, __interpose")))
    = {
          { .replacement = (void*)fake_IOConnectCallMethod,
              .original = (void*)IOConnectCallMethod },
          {
              .replacement = (void*)fake_IOAccelResourceCreate,
              .original = (void*)IOAccelResourceCreate,
          },
		  {
			  .replacement = (void*)fake_IOGPUResourceCreate,
			  .original = (void*)IOGPUResourceCreate,
		  },
      };

/*
 clang -Wall -dynamiclib -o flip.dylib flip_bufs.c -framework IOKit -F /System/Library/PrivateFrameworks/ -framework IOAccelerator -framework IOGPU -arch x86_64
 */
