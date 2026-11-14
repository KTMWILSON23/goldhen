/*
 * XXFWilson23 standalone ShellUI rainbow text module for PS4 14.02.
 *
 * The only native detour is the live-metadata-proven UI2.Label.OnPrepareRender
 * method.  A trampoline is installed only when every copied instruction is
 * position independent. Relative control flow and RIP-relative operands fail
 * closed instead of being copied unsafely.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct MonoDomain MonoDomain;
typedef struct MonoAssembly MonoAssembly;
typedef struct MonoImage MonoImage;
typedef struct MonoClass MonoClass;
typedef struct MonoMethod MonoMethod;
typedef struct MonoProperty MonoProperty;
typedef struct MonoThread MonoThread;
typedef struct MonoObject MonoObject;
typedef struct MonoString MonoString;
typedef struct MonoType MonoType;
typedef struct MonoArray MonoArray;
typedef struct MonoVTable MonoVTable;
typedef struct MonoClassField MonoClassField;
typedef struct MonoArrayType MonoArrayType;
typedef struct MonoMethodSignature MonoMethodSignature;
typedef struct MonoMethodDesc MonoMethodDesc;
typedef struct MonoGenericParam MonoGenericParam;
typedef struct MonoGenericClass MonoGenericClass;
typedef struct MonoGenericContainer MonoGenericContainer;
typedef struct MonoCustomMod MonoCustomMod;
typedef struct MonoMarshalType MonoMarshalType;
typedef struct MonoClassRuntimeInfo MonoClassRuntimeInfo;
typedef struct MonoClassExt MonoClassExt;
typedef struct MonoThreadsSync MonoThreadsSync;
typedef struct MonoRuntimeGenericContext MonoRuntimeGenericContext;
typedef struct MonoEvent MonoEvent;
typedef struct MonoArrayBounds MonoArrayBounds;
typedef struct MonoMethodNormal MonoMethodNormal;
typedef struct MonoMethodPInvoke MonoMethodPInvoke;
typedef struct MonoMethodInflated MonoMethodInflated;
typedef struct MonoGenericInst MonoGenericInst;
typedef struct MonoGenericContext MonoGenericContext;

#include <orbis/libmonovm.h>
#include <orbis/libkernel.h>
#include "HDE64.h"

#define RAINBOW_DIR "/user/data/xxfwilson23-rainbow"
#define RAINBOW_LOG RAINBOW_DIR "/rainbow-1402.log"
#define RAINBOW_INTERVAL_US 50000ULL
#define RAINBOW_HUE_STEPS 1536ULL
#define RAINBOW_HUE_ADVANCE 8ULL
#define RAINBOW_PALETTE_SIZE \
    (RAINBOW_HUE_STEPS / RAINBOW_HUE_ADVANCE)
#define RAINBOW_SEEN_CAPACITY 2048U
#define REL_JUMP_SIZE 5U
#define ABS_JUMP_SIZE 14U
#define MAX_PROLOGUE_SIZE 32U
#define MONO_TYPE_VOID 0x01
#define MONO_TYPE_U1 0x05
#define MONO_TYPE_VALUETYPE 0x11
#define MONO_METHOD_ATTR_STATIC 0x0010U

/* These are exported by libmonosgen but omitted/incorrectly typed in the
 * OpenOrbis v0.5.3 header. */
extern MonoClass *xxf_mono_object_get_class(MonoObject *object)
    __asm__("mono_object_get_class");
extern MonoMethodSignature *mono_method_signature(MonoMethod *method);
extern uint32_t mono_signature_get_param_count(MonoMethodSignature *signature);
extern MonoType *mono_signature_get_params(
    MonoMethodSignature *signature,
    void **iterator
);
extern MonoType *mono_signature_get_return_type(
    MonoMethodSignature *signature
);
extern int xxf_mono_type_get_type(MonoType *type)
    __asm__("mono_type_get_type");
extern uint32_t mono_method_get_flags(
    MonoMethod *method,
    uint32_t *implementation_flags
);
extern MonoClass *mono_method_get_class(MonoMethod *method);

typedef void (*OnPrepareRenderFn)(void *instance);

typedef struct SafeHook {
    uintptr_t target;
    uint8_t original[MAX_PROLOGUE_SIZE];
    uint8_t patch[REL_JUMP_SIZE];
    size_t stolen_length;
    void *trampoline;
    size_t trampoline_size;
    bool installed;
} SafeHook;

typedef struct RainbowRuntime {
    MonoDomain *domain;
    MonoThread *thread;
    MonoImage *pui;
    MonoClass *label;
    MonoClass *color;
    MonoMethod *set_text_color;
    MonoMethod *from_byte;
    MonoMethod *on_prepare_render;
} RainbowRuntime;

static int g_log_fd = -1;
static SafeHook g_hook;
static OnPrepareRenderFn g_original = NULL;
static uint32_t g_palette[RAINBOW_PALETTE_SIZE];
static uintptr_t g_seen_labels[RAINBOW_SEEN_CAPACITY];
static uint64_t g_seen_bucket = UINT64_MAX;
static MonoClass *g_color_class = NULL;
static MonoMethod *g_set_text_color = NULL;
static MonoMethod *g_from_byte = NULL;
static bool g_effect_enabled = false;
static bool g_effect_failed = false;
static bool g_hook_active = false;

static void log_line(const char *format, ...) {
    if (g_log_fd < 0) {
        return;
    }
    char line[384];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(line, sizeof(line) - 1U, format, args);
    va_end(args);
    if (length <= 0) {
        return;
    }
    if ((size_t)length >= sizeof(line) - 1U) {
        length = (int)sizeof(line) - 2;
    }
    line[length++] = '\n';
    size_t offset = 0;
    while (offset < (size_t)length) {
        size_t written = sceKernelWrite(
            g_log_fd,
            line + offset,
            (size_t)length - offset
        );
        if (written == 0 || written == (size_t)-1) {
            break;
        }
        offset += written;
    }
}

static void notify_user(const char *format, ...) {
    OrbisNotificationRequest request;
    memset(&request, 0, sizeof(request));
    request.type = NotificationRequest;
    request.targetId = -1;
    request.userId = -1;
    va_list args;
    va_start(args, format);
    vsnprintf(request.message, sizeof(request.message), format, args);
    va_end(args);
    sceKernelSendNotificationRequest(0, &request, sizeof(request), 0);
}

static long syscall_108(
    long pid,
    uintptr_t address,
    void *buffer,
    size_t length,
    long write
) {
    unsigned long result;
    unsigned char is_error;
    register long argument4 __asm__("r10") = (long)length;
    register long argument5 __asm__("r8") = write;
    __asm__ __volatile__(
        "syscall"
        : "=a"(result), "=@ccc"(is_error), "+r"(argument4),
          "+r"(argument5)
        : "a"(108L), "D"(pid), "S"((long)address), "d"(buffer)
        : "rcx", "r11", "memory"
    );
    return is_error ? -(long)result : (long)result;
}

static int process_rw(
    uintptr_t address,
    void *buffer,
    size_t length,
    bool write
) {
    if (!address || !buffer || !length) {
        return -1;
    }
    return (int)syscall_108(
        getpid(),
        address,
        buffer,
        length,
        write ? 1 : 0
    );
}

static void make_absolute_jump(
    uint8_t output[ABS_JUMP_SIZE],
    uintptr_t destination
) {
    static const uint8_t prefix[6] = {0xff, 0x25, 0, 0, 0, 0};
    memcpy(output, prefix, sizeof(prefix));
    memcpy(output + sizeof(prefix), &destination, sizeof(destination));
}

static bool is_return(const hde64s *decoded) {
    return decoded->opcode == 0xc2 || decoded->opcode == 0xc3 ||
           decoded->opcode == 0xca || decoded->opcode == 0xcb ||
           decoded->opcode == 0xcf;
}

static bool is_unsupported_control_flow(const hde64s *decoded) {
    if (is_return(decoded) || decoded->opcode == 0x9a ||
        decoded->opcode == 0xea || decoded->opcode == 0xcc ||
        decoded->opcode == 0xcd || decoded->opcode == 0xce ||
        decoded->opcode == 0xf4) {
        return true;
    }
    if (decoded->opcode == 0xff && (decoded->flags & F_MODRM) &&
        decoded->modrm_reg >= 2 && decoded->modrm_reg <= 5) {
        return true;
    }
    return decoded->opcode == 0x0f && decoded->opcode2 == 0x0b;
}

static int safe_prologue_size(const uint8_t *bytes, size_t available) {
    size_t offset = 0;
    while (offset < REL_JUMP_SIZE) {
        hde64s decoded;
        memset(&decoded, 0, sizeof(decoded));
        unsigned int length = hde64_disasm(bytes + offset, &decoded);
        if (!length || length != decoded.len || decoded.flags & F_ERROR) {
            return -1;
        }
        if (offset + length > available || decoded.flags & F_RELATIVE) {
            return -2;
        }
        if (is_unsupported_control_flow(&decoded)) {
            return -3;
        }
        if ((decoded.flags & F_MODRM) && decoded.modrm_mod == 0 &&
            decoded.modrm_rm == 5 && decoded.p_67 == 0) {
            return -4;
        }
        offset += length;
    }
    return offset <= MAX_PROLOGUE_SIZE ? (int)offset : -5;
}

static bool restore_hook_prefix(SafeHook *hook) {
    uint8_t verify[REL_JUMP_SIZE];
    bool restored = process_rw(
        hook->target,
        hook->original,
        REL_JUMP_SIZE,
        true
    ) == 0 && process_rw(
        hook->target,
        verify,
        sizeof(verify),
        false
    ) == 0 && memcmp(
        verify,
        hook->original,
        sizeof(verify)
    ) == 0;
    __builtin___clear_cache(
        (char *)hook->target,
        (char *)(hook->target + REL_JUMP_SIZE)
    );
    return restored;
}

static int install_hook(
    SafeHook *hook,
    uintptr_t target,
    uintptr_t replacement,
    OnPrepareRenderFn *original_out
) {
    if (!hook || !target || !replacement || !original_out) {
        return -1;
    }
    memset(hook, 0, sizeof(*hook));
    uint8_t snapshot[MAX_PROLOGUE_SIZE];
    if (process_rw(target, snapshot, sizeof(snapshot), false) != 0) {
        return -2;
    }
    /* Refuse to stack on any existing jump/detour. */
    if ((snapshot[0] == 0xff && snapshot[1] == 0x25) ||
        snapshot[0] == 0xe9 || snapshot[0] == 0xeb) {
        return -3;
    }
    intptr_t displacement = (intptr_t)replacement -
        (intptr_t)(target + REL_JUMP_SIZE);
    if (displacement < INT32_MIN || displacement > INT32_MAX) {
        return -4;
    }
    int decoded = safe_prologue_size(snapshot, sizeof(snapshot));
    if (decoded < (int)REL_JUMP_SIZE) {
        return -10 + decoded;
    }

    hook->target = target;
    hook->stolen_length = (size_t)decoded;
    memcpy(hook->original, snapshot, hook->stolen_length);
    hook->patch[0] = 0xe9;
    int32_t relative = (int32_t)displacement;
    memcpy(hook->patch + 1, &relative, sizeof(relative));
    hook->trampoline_size = hook->stolen_length + ABS_JUMP_SIZE;
    int result = sceKernelMmap(
        NULL,
        hook->trampoline_size,
        VM_PROT_ALL,
        0x1000 | 0x2,
        -1,
        0,
        &hook->trampoline
    );
    if (result < 0 || !hook->trampoline) {
        return -20;
    }
    memcpy(hook->trampoline, hook->original, hook->stolen_length);
    make_absolute_jump(
        (uint8_t *)hook->trampoline + hook->stolen_length,
        hook->target + hook->stolen_length
    );
    __builtin___clear_cache(
        (char *)hook->trampoline,
        (char *)hook->trampoline + hook->trampoline_size
    );

    __atomic_store_n(
        original_out,
        (OnPrepareRenderFn)hook->trampoline,
        __ATOMIC_RELEASE
    );
    if (process_rw(hook->target, hook->patch, REL_JUMP_SIZE, true) != 0) {
        uint8_t current[REL_JUMP_SIZE];
        bool unchanged = process_rw(
            hook->target,
            current,
            sizeof(current),
            false
        ) == 0 && memcmp(
            current,
            hook->original,
            sizeof(current)
        ) == 0;
        if (!unchanged && !restore_hook_prefix(hook)) {
            hook->installed = true;
            return -24;
        }
        __atomic_store_n(original_out, NULL, __ATOMIC_RELEASE);
        sceKernelMunmap(hook->trampoline, hook->trampoline_size);
        memset(hook, 0, sizeof(*hook));
        return -21;
    }
    uint8_t verify[REL_JUMP_SIZE];
    memset(verify, 0, sizeof(verify));
    if (process_rw(hook->target, verify, sizeof(verify), false) != 0 ||
        memcmp(verify, hook->patch, sizeof(verify)) != 0) {
        if (!restore_hook_prefix(hook)) {
            /* Keep the trampoline and original pointer resident in case the
             * target contains a complete jump despite the failed readback. */
            hook->installed = true;
            return -23;
        }
        __atomic_store_n(original_out, NULL, __ATOMIC_RELEASE);
        sceKernelMunmap(hook->trampoline, hook->trampoline_size);
        memset(hook, 0, sizeof(*hook));
        return -22;
    }
    __builtin___clear_cache(
        (char *)hook->target,
        (char *)(hook->target + REL_JUMP_SIZE)
    );
    hook->installed = true;
    return 0;
}

static MonoImage *open_pui(MonoDomain *domain) {
    const char *sandbox = sceKernelGetFsSandboxRandomWord();
    if (!sandbox) {
        return NULL;
    }
    static const char *suffixes[] = {
        "/common/lib/Sce.PlayStation.PUI.dll",
        "/common/lib/Sce.PlayStation.PUI.dll.sprx",
    };
    char path[260];
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
        snprintf(path, sizeof(path), "/%s%s", sandbox, suffixes[i]);
        MonoAssembly *assembly = mono_domain_assembly_open(domain, path);
        if (assembly) {
            MonoImage *image = mono_assembly_get_image(assembly);
            if (image) {
                log_line("PUI image opened: %s", path);
                return image;
            }
        }
    }
    return NULL;
}

static MonoObject *create_color(
    uint8_t red,
    uint8_t green,
    uint8_t blue,
    uint8_t alpha,
    MonoObject **exception
) {
    void *arguments[4] = {&red, &green, &blue, &alpha};
    MonoObject *color = mono_runtime_invoke(
        g_from_byte,
        NULL,
        arguments,
        exception
    );
    if (!color || (exception && *exception) ||
        xxf_mono_object_get_class(color) != g_color_class) {
        return NULL;
    }
    return color;
}

static bool type_is(MonoType *type, int kind, MonoClass *klass) {
    if (!type || type->byref || xxf_mono_type_get_type(type) != kind) {
        return false;
    }
    return !klass || mono_class_from_mono_type(type) == klass;
}

static bool has_void_return(MonoMethodSignature *signature) {
    return signature && type_is(
        mono_signature_get_return_type(signature),
        MONO_TYPE_VOID,
        NULL
    );
}

static bool method_is_static(MonoMethod *method) {
    uint32_t implementation_flags = 0;
    return (mono_method_get_flags(method, &implementation_flags) &
            MONO_METHOD_ATTR_STATIC) != 0;
}

static bool validate_signatures(const RainbowRuntime *runtime) {
    MonoMethodSignature *from = mono_method_signature(runtime->from_byte);
    MonoMethodSignature *setter = mono_method_signature(
        runtime->set_text_color
    );
    MonoMethodSignature *prepare = mono_method_signature(
        runtime->on_prepare_render
    );
    if (!from || !setter || !prepare) {
        return false;
    }
    if (!method_is_static(runtime->from_byte) ||
        method_is_static(runtime->set_text_color) ||
        method_is_static(runtime->on_prepare_render)) {
        return false;
    }
    if (mono_signature_get_param_count(from) != 4U ||
        mono_signature_get_param_count(setter) != 1U ||
        mono_signature_get_param_count(prepare) != 0U) {
        return false;
    }
    void *iterator = NULL;
    for (unsigned int i = 0; i < 4U; ++i) {
        MonoType *parameter = mono_signature_get_params(from, &iterator);
        if (!type_is(parameter, MONO_TYPE_U1, NULL)) {
            return false;
        }
    }
    if (!type_is(
            mono_signature_get_return_type(from),
            MONO_TYPE_VALUETYPE,
            runtime->color
        )) {
        return false;
    }
    iterator = NULL;
    MonoType *color_parameter = mono_signature_get_params(setter, &iterator);
    if (!type_is(color_parameter, MONO_TYPE_VALUETYPE, runtime->color) ||
        !has_void_return(setter) || !has_void_return(prepare)) {
        return false;
    }
    return mono_method_get_class(runtime->from_byte) == runtime->color &&
        mono_method_get_class(runtime->set_text_color) == runtime->label &&
        mono_method_get_class(runtime->on_prepare_render) == runtime->label;
}

static void rainbow_rgb(
    uint64_t bucket,
    uint8_t *red,
    uint8_t *green,
    uint8_t *blue
) {
    uint32_t hue = (uint32_t)(
        (bucket * RAINBOW_HUE_ADVANCE) % RAINBOW_HUE_STEPS
    );
    uint32_t sector = hue >> 8;
    uint8_t offset = (uint8_t)(hue & 0xffU);
    uint8_t falling = (uint8_t)(255U - offset);
    *red = *green = *blue = 0;
    switch (sector) {
        case 0: *red = 255; *green = offset; break;
        case 1: *red = falling; *green = 255; break;
        case 2: *green = 255; *blue = offset; break;
        case 3: *green = falling; *blue = 255; break;
        case 4: *red = offset; *blue = 255; break;
        default: *red = 255; *blue = falling; break;
    }
}

static void release_palette(void) {
    for (uint32_t i = 0; i < RAINBOW_PALETTE_SIZE; ++i) {
        if (g_palette[i]) {
            mono_gchandle_free(g_palette[i]);
            g_palette[i] = 0;
        }
    }
}

static bool build_palette(void) {
    memset(g_palette, 0, sizeof(g_palette));
    for (uint32_t i = 0; i < RAINBOW_PALETTE_SIZE; ++i) {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        rainbow_rgb(i, &red, &green, &blue);
        MonoObject *exception = NULL;
        MonoObject *color = create_color(
            red,
            green,
            blue,
            255U,
            &exception
        );
        if (!color || exception) {
            release_palette();
            return false;
        }
        uint32_t handle = mono_gchandle_new(color, 1);
        MonoObject *rooted = handle
            ? mono_gchandle_get_target(handle)
            : NULL;
        if (!rooted || xxf_mono_object_get_class(rooted) != g_color_class) {
            if (handle) {
                mono_gchandle_free(handle);
            }
            release_palette();
            return false;
        }
        g_palette[i] = handle;
    }
    return true;
}

static void fail_runtime(void) {
    if (__atomic_exchange_n(&g_effect_failed, true, __ATOMIC_ACQ_REL)) {
        return;
    }
    __atomic_store_n(&g_effect_enabled, false, __ATOMIC_RELEASE);
    notify_user("XXFWilson23: rainbow runtime validation failed");
}

static bool mark_label_for_bucket(uintptr_t label, uint64_t bucket) {
    if (g_seen_bucket != bucket) {
        memset(g_seen_labels, 0, sizeof(g_seen_labels));
        g_seen_bucket = bucket;
    }
    uint64_t mixed = (uint64_t)label;
    mixed ^= mixed >> 33;
    mixed *= 0xff51afd7ed558ccdULL;
    mixed ^= mixed >> 33;
    uint32_t index = (uint32_t)mixed & (RAINBOW_SEEN_CAPACITY - 1U);
    for (uint32_t probe = 0; probe < RAINBOW_SEEN_CAPACITY; ++probe) {
        uintptr_t *slot = &g_seen_labels[
            (index + probe) & (RAINBOW_SEEN_CAPACITY - 1U)
        ];
        if (*slot == label) {
            return false;
        }
        if (*slot == 0) {
            *slot = label;
            return true;
        }
    }
    /* A full table is not a validation failure; applying again is safe. */
    return true;
}

static void on_prepare_render_hook(void *instance) {
    OnPrepareRenderFn original = __atomic_load_n(
        &g_original,
        __ATOMIC_ACQUIRE
    );
    if (!original) {
        return;
    }
    if (!instance || !__atomic_load_n(&g_effect_enabled, __ATOMIC_ACQUIRE) ||
        __atomic_exchange_n(&g_hook_active, true, __ATOMIC_ACQ_REL)) {
        original(instance);
        return;
    }

    uint64_t bucket = sceKernelGetProcessTime() / RAINBOW_INTERVAL_US;
    if (!mark_label_for_bucket((uintptr_t)instance, bucket)) {
        original(instance);
        __atomic_store_n(&g_hook_active, false, __ATOMIC_RELEASE);
        return;
    }

    uint32_t label_handle = mono_gchandle_new((MonoObject *)instance, 1);
    MonoObject *label = label_handle
        ? mono_gchandle_get_target(label_handle)
        : NULL;
    bool applied = false;
    if (label) {
        uint32_t index = (uint32_t)(bucket % RAINBOW_PALETTE_SIZE);
        MonoObject *color = mono_gchandle_get_target(g_palette[index]);
        void *color_data = color ? mono_object_unbox(color) : NULL;
        if (color_data) {
            void *arguments[1] = {color_data};
            MonoObject *exception = NULL;
            mono_runtime_invoke(
                g_set_text_color,
                label,
                arguments,
                &exception
            );
            applied = exception == NULL;
            label = mono_gchandle_get_target(label_handle);
        }
    }
    if (!applied) {
        fail_runtime();
    }
    if (label) {
        original(label);
    } else {
        /* No managed call ran without a pinned target, so the incoming
         * pointer is still valid for the mandatory original invocation. */
        original(instance);
    }
    if (label_handle) {
        mono_gchandle_free(label_handle);
    }
    __atomic_store_n(&g_hook_active, false, __ATOMIC_RELEASE);
}

static int resolve_and_install(RainbowRuntime *runtime) {
    memset(runtime, 0, sizeof(*runtime));
    runtime->domain = mono_get_root_domain();
    if (!runtime->domain) {
        return -1;
    }
    runtime->thread = mono_thread_attach(runtime->domain);
    if (!runtime->thread) {
        return -2;
    }
    runtime->pui = open_pui(runtime->domain);
    if (!runtime->pui) {
        return -3;
    }
    runtime->label = mono_class_from_name(
        runtime->pui,
        "Sce.PlayStation.PUI.UI2",
        "Label"
    );
    runtime->color = mono_class_from_name(
        runtime->pui,
        "Sce.PlayStation.PUI",
        "UIColor"
    );
    if (!runtime->label || !runtime->color) {
        return -4;
    }
    MonoProperty *text_color = mono_class_get_property_from_name(
        runtime->label,
        "TextColor"
    );
    runtime->set_text_color = text_color
        ? mono_property_get_set_method(text_color)
        : NULL;
    runtime->from_byte = mono_class_get_method_from_name(
        runtime->color,
        "FromByte",
        4
    );
    runtime->on_prepare_render = mono_class_get_method_from_name(
        runtime->label,
        "OnPrepareRender",
        0
    );
    if (!runtime->set_text_color || !runtime->from_byte ||
        !runtime->on_prepare_render) {
        return -5;
    }

    g_color_class = runtime->color;
    g_set_text_color = runtime->set_text_color;
    g_from_byte = runtime->from_byte;
    if (!validate_signatures(runtime)) {
        return -6;
    }
    if (!build_palette()) {
        return -7;
    }
    void *target = mono_aot_get_method(
        runtime->domain,
        runtime->on_prepare_render
    );
    if (!target) {
        release_palette();
        return -8;
    }
    log_line("Resolved UI2.Label.OnPrepareRender at %p", target);
    int hook_result = install_hook(
        &g_hook,
        (uintptr_t)target,
        (uintptr_t)on_prepare_render_hook,
        &g_original
    );
    if (hook_result != 0) {
        release_palette();
    }
    return hook_result;
}

static void *initialise_thread(void *unused) {
    (void)unused;
    sceKernelUsleep(1000000);
    RainbowRuntime runtime;
    int result = resolve_and_install(&runtime);
    if (result == 0) {
        __atomic_store_n(&g_effect_enabled, true, __ATOMIC_RELEASE);
        log_line("Rainbow enabled with one verified render-thread hook");
        notify_user("XXFWilson23: rainbow ShellUI text enabled");
    } else {
        log_line("Rainbow initialisation failed: %d", result);
        notify_user("XXFWilson23: rainbow module failed (%d)", result);
    }
    if (runtime.thread) {
        mono_thread_detach(runtime.thread);
    }
    if (g_log_fd >= 0) {
        sceKernelFsync(g_log_fd);
    }
    return NULL;
}

int module_start(size_t argc, const void *args) {
    (void)argc;
    (void)args;
    sceKernelMkdir(RAINBOW_DIR, 0777);
    g_log_fd = sceKernelOpen(RAINBOW_LOG, 0x0601, 0666);
    log_line("XXFWilson23 standalone rainbow module loaded");
    log_line("LOG_PATH=%s", RAINBOW_LOG);

    OrbisPthread thread;
    int result = scePthreadCreate(
        &thread,
        NULL,
        initialise_thread,
        NULL,
        "xxf_rainbow_init"
    );
    if (result != 0) {
        notify_user("XXFWilson23: rainbow thread failed (0x%08x)", result);
        if (g_log_fd >= 0) {
            sceKernelClose(g_log_fd);
            g_log_fd = -1;
        }
        return result;
    }
    scePthreadDetach(thread);
    return 0;
}

int module_stop(size_t argc, const void *args) {
    (void)argc;
    (void)args;
    /* A live render-thread detour cannot be removed without quiescing
     * SceShellUI. Refuse unload and keep the module resident until reboot. */
    log_line("Unload refused: reboot is required to remove the live hook");
    notify_user("XXFWilson23: rainbow module remains active until reboot");
    return -1;
}
