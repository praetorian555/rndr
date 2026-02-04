# Hardware

## Table of Contents

---

1. [Introduction](#introduction)
2. [Hardware Components](#hardware-components)
   1. [Command Processors](#command-processors)
   2. [Graphics Processing Cluster](#graphics-processing-cluster)
   3. [Streaming Multiprocessor](#streaming-multiprocessor-compute-unit-in-amd)
   4. [Raster Engine](#raster-engine-rasterization-unit)
   5. [Render Output Processors](#render-output-processors)
3. [Memory](#memory)
   1. [Bandwidth](#bandwidth)
   2. [Latency](#latency)
4. [References](#references)

## Introduction

---

This document will try to gather in one place all that is known about modern GPU architectures and how these hardware
elements map to modern graphics APIs. It will focus on last 10 years or so, on both NVIDIA and AMD GPUs.

## Hardware Components

---

### Command Processors

GPUs have multiple command processors for different types of work. They receive commands from CPU driver and then
dispatch the work across GPU resources. There are:
* Graphics command processor which handles 3D work. It can also handle compute work as well.
* Compute command processor which handles general purpose compute work. Also known as async compute.
* Copy command processor which uses DMAs to perform movement of data in memory.
* Video encoding/decoding processor which can decode and play video files.

It is important to note that even though these are separate processors they might contest around the same resources, for
example CUDA cores.

### Graphics Processing Cluster

It's a high-level organizational unit that stores a Raster Engine and multiple streaming multiprocessors. One GPU can
have multiple of these.

### Streaming Multiprocessor (Compute Unit in AMD)

This is a component used for parallel processing in the GPU. It contains compute cores for generic programming and
graphics shaders, tensor cores for AI workload or DLSS, and ray tracing cores for acceleration of ray tracing
algorithms. Compute cores or CUDA cores are the most common ones.

General purpose work that is scheduled on the CUDA cores is usually scheduled as a block that consists of threads.
Each block is scheduled on exactly one streaming multiprocessor. Each thread is executed on one CUDA core. Threads
are not scheduled individually but rather in groups called warps that can consist from 32 or 64 threads. All threads in
one warp are executed in SIMD model.

Note that graphics shaders such as vertex and fragment shaders are also executed on CUDA cores and are internally
scheduled as general purpose CUDA work.

Streaming multiprocessors also have texture units or texture samplers which are dedicated hardware used to sample
textures from memory based on given coordinates, properly handling wrapping as well as filtering of the texels.

It has L1 cache/shared memory block as well which can be configured how to be used. Each CUDA core also has local
instruction cache.

### Raster Engine (Rasterization Unit)

Fixed-function hardware component that converts triangles as outputted by vertex or mesh shaders to fragments to be processed
using fragment shaders.

### Render Output Processors

This is another fixed-function hardware component that handles depth/stencil testing, blending and framebuffer writes.
It has local color cache as well as depth/stencil cache.

## Memory

---

### Bandwidth

Discrete GPUs have their own working memory called VRAM. This memory and its connection to the GPU are optimized to
provide maximum bandwidth in order to feed a lot of processing cores present on the GPU with a sufficient rate. They
achieve this by using wider buses compared to system memory buses, but also being able to transfer more data per pin.
Total bandwidth that VRAM has is defined as a multiply of how many transactions each pin can do in one seconds and how
wide the bus is in bits. For example if we can do 8 GT/s and have 128 bit bus width we have 128 GB/s bandwidth.

GPU memory and system memory are separated, but on modern devices both GPU can see system memory and CPU can see GPU
memory. From a point of view of a GPU, VRAM is local memory and system memory is remote memory.

CPU has access to the GPU memory through PCIe connection with which the GPU is connected to the motherboard. What
exactly the PCIe version is used and what number of lanes it has is defined by what GPU, motherboard, PCIe slot and CPU
are present. For example, PCIe version 4.0 can do 16 GT/s per lane and if we have 8 lanes we get 16 GB/s of bandwidth,
which is much lower than VRAM bandwidth. This means that great care must be taken where data is stored and how much it is
being transferred between CPU and GPU.

### Latency

Both CPUs and GPUs have some form of caches that they utilize. In the context of memory that is both visible by CPU and
GPU they differ in the sense that GPU always cache all memory accesses while CPUs might not.

Here we can define four types of memory:

* **Local Visible Memory** - VRAM directly accessible by the CPU.
* **Local Invisible Memory** - VRAM not directly accessible by the CPU.
* **Remote Cached Memory** - system memory accessible by the GPU, and cached by the CPU.
* **Remote Uncached Memory** - system memory accessible by the GPU, but not cached by the CPU.

From this we can see that GPUs will cache all memory accesses while CPUs will only cache Remote Cached Memory. This means
that CPU reads from uncached memory will be slower and coherency of CPU cached memory with respect to GPU may need CPU
cache flushes and invalidations at the appropriate time, or some snooping protocols in place.

In regard to latency GPU reads from VRAM in general will have higher latency then CPU reads from system memory since
GPUs are optimized to have higher bandwidth at the cost of memory latency.

### Resizable BAR Support

When device is connected to the system using PCIe port, it needs to be mapped to IO port or memory address space so that
drivers are able to communicate to underlying hardware or access its physical memory if possible. This is done by programming
Base Address Registers (BARs). The available local visible memory is determined by this BAR configuration.

Until recently this memory was limited to the size of 256 MB, but with recent graphics cards like Blackwell architecture
from NVIDIA, whole VRAM is local visible memory and local invisible memory is no longer used.

During application runtime, every frame, there is a lot of data that needs to be moved between CPU and GPU. In case that
the whole VRAM is locally visible memory, we can simply copy the data from the CPU into this memory. In case that it is
limited to 256 MB, we need to use different approaches. One approach is to put data into remote memory (typically remote
uncached) and another is to write to remote memory and then use GPU copy command to move this data to local invisible 
memory.

Note that it's not always possible to store all the application data in the local memory, so it is often best to use
local memory as a cached memory of the one stored in the remote memory.

When working with textures it is still needed to use copy commands from the DMA engine since textures need  to be
stored in a optimal format for the specific GPU we are using so some conversion from storage format to the optimal
format needs to happen in GPU hardware. In this case we have to use staging buffers and copy commands to properly
setup texture in the local memory.

### Cache

In order to properly utilize the cache present on our discrete GPUs we want to maximize the hit rate, which is defined
as ratio between number of data accesses that can be handled by the data in cache and number of all data accesses.

Modern devices will usually have L1 cache in each streaming multiprocessor. This one is shared by all the cores inside
this SM. Additionally, each core has separate instruction cache. Finally, there is L2 cache that is shared between all
the SMs on the device. L2 cache is also coherent, which is not the case for L1. When writing shaders and 
kernels you can explicitly mark a buffer or a texture as **coherent** to make sure that all reads and writes to it will go
directly from L2, bypassing any L1 cache. This is potentially slower since local cache is not used but is needed where
different SMs are processing same collection of data, and they need to see proper state of the whole memory during 
processing.

Shared memory is very similar to L1 cache memory, but it is used manually by a programmer to cache some data that is
frequently used by the program. On NVIDIA GPUs, L1 and shared memory, well, share same space and user can configure how
much is allocated to the L1 and to the shared memory.

ROPs also have their own caches, usually for both color and depth/stencil render targets. They are also not coherent
and usually are backed up by the L2 cache. This is true for immediate mode renderer GPUs which are common in desktop
setups. On mobile, GPUs use tile-based renderer where ROPs don't have the cache.

## References

---

* [Discussion on the future of graphics API](https://www.sebastianaaltonen.com/blog/no-graphics-api)
* [NVIDIA Turing Architecture](https://images.nvidia.com/aem-dam/en-zz/Solutions/design-visualization/technologies/turing-architecture/NVIDIA-Turing-Architecture-Whitepaper.pdf)
* [NVIDIA Ampere Architecture](https://www.nvidia.com/content/PDF/nvidia-ampere-ga-102-gpu-architecture-whitepaper-v2.pdf)
* [NVIDIA Ada Architecture](https://images.nvidia.com/aem-dam/Solutions/geforce/ada/nvidia-ada-gpu-architecture.pdf)
* [NVIDIA Blackwell Architecture](https://images.nvidia.com/aem-dam/Solutions/geforce/blackwell/nvidia-rtx-blackwell-gpu-architecture.pdf)
* [AMD GCN Architecture](https://www.techpowerup.com/gpu-specs/docs/amd-gcn1-architecture.pdf)
* [AMD RDNA Architecture](https://gpuopen.com/download/RDNA_Architecture_public.pdf)
* [GPU Cache Hierarchy: Understanding L1, L2 and VRAM](https://charlesgrassi.dev/blog/gpu-cache-hierarchy/)
* [Understanding GPU Caches](https://www.rastergrid.com/blog/gpu-tech/2021/01/understanding-gpu-caches/)
* [MSAA Overview](https://therealmjp.github.io/posts/msaa-overview/)


