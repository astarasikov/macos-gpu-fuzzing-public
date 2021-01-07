#include <IOKit/IOKitLib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct BufferDesc {
    void* start;
    size_t length;
	void* cache;
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
    if (count < 10 || count % 4 != 0 || !gBuffersStored) {
        //wait until at least one buffer is ready
		//avoid locking up the machine for too long
		
		if (count == 1) {
			srand(time(NULL));
		}
        return;
    }

	if (0) {
		dumpGPUData();
		return;
	}

    fprintf(stderr, "%s: CORRUPTING GPU BUFFERS total=%zu\n",
			__func__, gBuffersStored);
    for (size_t i = 0; i < gBuffersStored; i++) {
        struct BufferDesc desc = gBufferDescs[i];
		//most apps seem to have two buffers: one with GPU code/data
		//and the other one with what looks like GPU VAs. I think
		//the second one is the ring buffer structures, and corrupting them
		//locks up the machine without producing any glitches
		if (((uint32_t*)desc.start)[0x10] < 0x10000) {
			//we can instead check "gpu_va" when resources are created
			//and skip the buffers which are persistently mapped
			//continue;
		}

		if (!desc.cache) {
			//save a good buffer
			desc.cache = malloc(desc.length);
			memcpy(desc.cache, desc.start, desc.length);
		}
		else {
			//restore a good buffer after the previous
			//fuzz iteration
			memcpy(desc.start, desc.cache, desc.length);
		}

        for (size_t j = 0; j < 32; j++) {
			size_t offset = 0x0;
			size_t max = (0x9000 - offset) / sizeof(int);
			size_t idx = (rand() % max) + offset;
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
		if (!desc.start || !desc.length || desc.length > 0x100000)
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

	if (gpu_va || !gpu_data || !gpu_size) {
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

	corruptGPUData();

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
