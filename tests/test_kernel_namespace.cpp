/**
 * MultiBackendKernelBuilder's generated namespace is the caller's.
 *
 * The goldens below are the output of the commit before the namespace became a parameter, 6874f3d,
 * for the same input, with its namespace root replaced by @NS@. Each render is compared with its
 * golden whole, so a change anywhere else in any of the five backends fails here.
 *
 * Nothing in this library may name a consumer, in its source or in what it renders, because a
 * consumer's product can carry this module without carrying that consumer. `kConsumerName` is the
 * name that used to be built in; this file is a test and never reaches a library or a binary a
 * consumer ships.
 */

// Textual stdlib includes before the module import (see examples/fluent_codegen.cpp).
#include <algorithm>
#include <cassert>
#include <cctype>
#include <iostream>
#include <map>
#include <string>
#include <string_view>

import logger.code_generator;

using namespace logger::codegen;

namespace {

using Golden = std::map<std::string, std::string>;

constexpr std::string_view kConsumerName = "sensen";
constexpr std::string_view kDefaultRoot = "logger::generated";
constexpr std::string_view kCallerRoot = "acme::kernels";

auto const kGoldenDefault = Golden{
    {"cpu_simd",
     R"GOLD(// CPU Multi-Register SIMD (AVX-512 / AVX2 / AMX / NEON) 1M-Context Kernel (qwen38_27b)
#include <immintrin.h>
#include <cstddef>
namespace @NS@::cpu_simd {
static constexpr std::size_t kContextLength = 1048576;
__attribute__((target("avx512f,avx512bw,avx512vl,avx512vnni,avx512bf16,fma"))) qwen38_27b void run_avx512_multiregister(float* dst, const float* src, std::size_t n) noexcept {
    std::size_t i = 0;
    for (; i + 64 <= n; i += 64) {
        __m512 z0 = _mm512_loadu_ps(src + i);
        __m512 z1 = _mm512_loadu_ps(src + i + 16);
        __m512 z2 = _mm512_loadu_ps(src + i + 32);
        __m512 z3 = _mm512_loadu_ps(src + i + 48);
        _mm512_storeu_ps(dst + i, z0);
        _mm512_storeu_ps(dst + i + 16, z1);
        _mm512_storeu_ps(dst + i + 32, z2);
        _mm512_storeu_ps(dst + i + 48, z3);
    }
}
} // namespace @NS@::cpu_simd
)GOLD"},
    {"cuda", R"GOLD(// CUDA 1M-Context Kernel (qwen38_27b, YARN, llq_fused/llq_fused)
#include <cuda_fp16.h>
#include <cstdint>
namespace @NS@::cuda {
static constexpr std::size_t kContextLength = 1048576;
static constexpr std::size_t kOrigContextLength = 131072;
static constexpr double kRopeScale = 8.000000;
extern "C" __global__ void qwen38_27b_yarn_llq_kernel(float* __restrict__ q, const float* __restrict__ k, std::size_t seq_pos) {
    const std::size_t tid = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (tid < 128) { q[tid] *= static_cast<float>(1.000000); }
}
} // namespace @NS@::cuda
)GOLD"},
    {"mlx", R"GOLD(# Apple MLX Unified-Memory Metal Kernel (qwen38_27b, YARN, 1048576 tokens)
import mlx.core as mx
MODEL_NAME = "qwen38_27b"
CONTEXT_LENGTH = 1048576
WEIGHT_QUANT = "llq_fused"
KV_QUANT = "llq_fused"
)GOLD"},
    {"triton", R"GOLD(# OpenAI Triton 1M-Context Kernel (qwen38_27b, YARN, 1048576 tokens)
import triton
import triton.language as tl
MODEL_NAME = "qwen38_27b"
CONTEXT_LENGTH = 1048576
WEIGHT_QUANT = "llq_fused"
KV_QUANT = "llq_fused"
)GOLD"},
    {"vulkan", R"GOLD(#version 460
#extension GL_KHR_cooperative_matrix : enable
// Vulkan Compute Shader (qwen38_27b, YARN, 1048576 tokens)
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;
layout(set = 0, binding = 0) buffer InOutBuf { float data[]; } buf;
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < 128u) { buf.data[idx] *= float(1.000000); }
}
)GOLD"},
};

auto const kGoldenConfigured = Golden{
    {"cpu_simd",
     R"GOLD(// CPU Multi-Register SIMD (AVX-512 / AVX2 / AMX / NEON) 1M-Context Kernel (qwen3_8_27b)
#include <immintrin.h>
#include <cstddef>
namespace @NS@::cpu_simd {
static constexpr std::size_t kContextLength = 1048576;
__attribute__((target("avx512f,avx512bw,avx512vl,avx512vnni,avx512bf16,fma"))) qwen3_8_27b void run_avx512_multiregister(float* dst, const float* src, std::size_t n) noexcept {
    std::size_t i = 0;
    for (; i + 64 <= n; i += 64) {
        __m512 z0 = _mm512_loadu_ps(src + i);
        __m512 z1 = _mm512_loadu_ps(src + i + 16);
        __m512 z2 = _mm512_loadu_ps(src + i + 32);
        __m512 z3 = _mm512_loadu_ps(src + i + 48);
        _mm512_storeu_ps(dst + i, z0);
        _mm512_storeu_ps(dst + i + 16, z1);
        _mm512_storeu_ps(dst + i + 32, z2);
        _mm512_storeu_ps(dst + i + 48, z3);
    }
}
} // namespace @NS@::cpu_simd
)GOLD"},
    {"cuda", R"GOLD(// CUDA 1M-Context Kernel (qwen3_8_27b, YARN, llq_fused/llq_fused)
#include <cuda_fp16.h>
#include <cstdint>
namespace @NS@::cuda {
static constexpr std::size_t kContextLength = 1048576;
static constexpr std::size_t kOrigContextLength = 131072;
static constexpr double kRopeScale = 8.000000;
extern "C" __global__ void qwen3_8_27b_yarn_llq_kernel(float* __restrict__ q, const float* __restrict__ k, std::size_t seq_pos) {
    const std::size_t tid = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (tid < 256) { q[tid] *= static_cast<float>(1.207944); }
}
} // namespace @NS@::cuda
)GOLD"},
    {"mlx", R"GOLD(# Apple MLX Unified-Memory Metal Kernel (qwen3_8_27b, YARN, 1048576 tokens)
import mlx.core as mx
MODEL_NAME = "qwen3_8_27b"
CONTEXT_LENGTH = 1048576
WEIGHT_QUANT = "llq_fused"
KV_QUANT = "llq_fused"
)GOLD"},
    {"triton", R"GOLD(# OpenAI Triton 1M-Context Kernel (qwen3_8_27b, YARN, 1048576 tokens)
import triton
import triton.language as tl
MODEL_NAME = "qwen3_8_27b"
CONTEXT_LENGTH = 1048576
WEIGHT_QUANT = "llq_fused"
KV_QUANT = "llq_fused"
)GOLD"},
    {"vulkan", R"GOLD(#version 460
#extension GL_KHR_cooperative_matrix : enable
// Vulkan Compute Shader (qwen3_8_27b, YARN, 1048576 tokens)
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;
layout(set = 0, binding = 0) buffer InOutBuf { float data[]; } buf;
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < 256u) { buf.data[idx] *= float(1.207944); }
}
)GOLD"},
};

/// The default-constructed builder: `kGoldenDefault`'s input.
auto defaultBuilder() -> MultiBackendKernelBuilder {
    return MultiBackendKernelBuilder{};
}

/// Every setter that feeds these templates: `kGoldenConfigured`'s input.
auto configuredBuilder() -> MultiBackendKernelBuilder {
    MultiBackendKernelBuilder builder{};
    builder.setModelName("qwen3.8-27b")
        .setArchitectureFamily("qwen3_5")
        .setDimensions(5120, 64, 24, 4, 256)
        .setContextLength(1048576, 131072)
        .setQuantization("llq_fused", "llq_fused")
        .setRopeScheme("yarn", 8.0)
        .setYarnParameters(32.0, 1.0, 1.207944, 1.0, true)
        .setAdapterMode("training_free_inference", true);
    return builder;
}

auto withRoot(std::string text, std::string_view root) -> std::string {
    constexpr std::string_view marker = "@NS@";
    for (auto at = text.find(marker); at != std::string::npos;
         at = text.find(marker, at + root.size())) {
        text.replace(at, marker.size(), root);
    }
    return text;
}

auto lowered(std::string text) -> std::string {
    std::ranges::transform(text, text.begin(), [](unsigned char c) -> char {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

auto assertMatchesGolden(MultiBackendKernelBuilder const& builder, Golden const& golden,
                         std::string_view root) -> void {
    auto const rendered = builder.renderAllBackends();
    assert(rendered.size() == golden.size());
    for (auto const& [backend, want] : golden) {
        assert(rendered.at(backend) == withRoot(want, root));
    }
}

auto test_caller_namespace_appears() -> void {
    std::cout << "Testing the caller's namespace opens and closes both kernels...\n";
    auto builder = defaultBuilder();
    auto& chained = builder.setGeneratedNamespace(kCallerRoot);
    assert(&chained == &builder);

    auto const cuda = builder.renderCudaKernel();
    assert(cuda.contains("\nnamespace acme::kernels::cuda {\n"));
    assert(cuda.ends_with("\n} // namespace acme::kernels::cuda\n"));

    auto const simd = builder.renderCpuMultiregisterSimdKernel();
    assert(simd.contains("\nnamespace acme::kernels::cpu_simd {\n"));
    assert(simd.ends_with("\n} // namespace acme::kernels::cpu_simd\n"));
    std::cout << "  ✓ passed\n";
}

auto test_default_is_the_librarys_own() -> void {
    std::cout << "Testing an unset namespace is this library's own...\n";
    auto const builder = defaultBuilder();
    assert(builder.renderCudaKernel().contains("\nnamespace logger::generated::cuda {\n"));
    assert(builder.renderCpuMultiregisterSimdKernel().contains(
        "\nnamespace logger::generated::cpu_simd {\n"));
    std::cout << "  ✓ passed\n";
}

auto test_no_output_names_a_consumer() -> void {
    std::cout << "Testing no backend's output names a consumer...\n";
    auto caller = configuredBuilder();
    caller.setGeneratedNamespace(kCallerRoot);
    for (auto const& builder : {defaultBuilder(), configuredBuilder(), caller}) {
        for (auto const& [backend, text] : builder.renderAllBackends()) {
            assert(!lowered(text).contains(kConsumerName));
        }
    }
    std::cout << "  ✓ passed\n";
}

auto test_rest_of_output_is_byte_identical() -> void {
    std::cout << "Testing the rest of every backend is byte-identical to the previous commit...\n";
    assertMatchesGolden(defaultBuilder(), kGoldenDefault, kDefaultRoot);
    assertMatchesGolden(configuredBuilder(), kGoldenConfigured, kDefaultRoot);

    auto caller_default = defaultBuilder();
    caller_default.setGeneratedNamespace(kCallerRoot);
    assertMatchesGolden(caller_default, kGoldenDefault, kCallerRoot);

    auto caller_configured = configuredBuilder();
    caller_configured.setGeneratedNamespace(kCallerRoot);
    assertMatchesGolden(caller_configured, kGoldenConfigured, kCallerRoot);
    std::cout << "  ✓ passed\n";
}

auto test_namespace_text_is_data() -> void {
    std::cout
        << "Testing a namespace that is not a qualified name cannot break the declaration...\n";
    assert(toQualifiedIdentifier("acme::kernels") == "acme::kernels");
    assert(toQualifiedIdentifier("acme") == "acme");
    assert(toQualifiedIdentifier("") == "_");
    assert(toQualifiedIdentifier("::") == "_::_");
    assert(toQualifiedIdentifier("a::") == "a::_");
    assert(toQualifiedIdentifier("::a") == "_::a");
    assert(toQualifiedIdentifier("a:b") == "a_b");
    assert(toQualifiedIdentifier("9a::b c") == "_9a::b_c");

    auto builder = defaultBuilder();
    builder.setGeneratedNamespace("x { int y; } namespace {{model_name}}");
    auto const cuda = builder.renderCudaKernel();
    assert(cuda.contains("\nnamespace x___int_y____namespace___model_name__::cuda {\n"));
    assert(!cuda.contains("int y"));
    std::cout << "  ✓ passed\n";
}

} // namespace

auto main() -> int {
    test_caller_namespace_appears();
    test_default_is_the_librarys_own();
    test_no_output_names_a_consumer();
    test_rest_of_output_is_byte_identical();
    test_namespace_text_is_data();
    std::cout << "All kernel namespace tests passed\n";
    return 0;
}
