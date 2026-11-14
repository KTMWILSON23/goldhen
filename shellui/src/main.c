/*
 * XXFWilson23 volatile ShellUI rainbow module.
 *
 * This file deliberately contains no unverified 14.02 tree traversal or
 * prologue detour. It attaches to the existing Mono runtime, proves the PUI
 * members needed by the final effect, and stops at a compile-time validation
 * gate until the live metadata capture supplies the exact root/child path.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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
typedef struct MonoTableInfo MonoTableInfo;

#include <orbis/libmonovm.h>
#include <orbis/libkernel.h>

#ifndef XXF_RAINBOW_COLLECTOR
#define XXF_RAINBOW_COLLECTOR 1
#endif

#define EVIDENCE_DIR "/user/data/xxfwilson23-rainbow"
#define LOG_PATH EVIDENCE_DIR "/pui-metadata-1402.txt"
#define EVIDENCE_MAX_BYTES (512U * 1024U)
#define MONO_TABLE_TYPEDEF 0x02
#define MONO_TOKEN_TYPE_DEF 0x02000000U
#define MAX_RELEVANT_CLASSES 512U
#define MAX_PROPERTIES_PER_CLASS 96U
#define MAX_METHODS_PER_CLASS 192U
#define RAINBOW_PENDING_METADATA (-1402)

/* libmonosgen exports these metadata APIs, but the OpenOrbis header only
 * declares the smaller embedding subset. Keep their standard Mono ABI here. */
extern const MonoTableInfo *mono_image_get_table_info(MonoImage *image, int table_id);
extern int mono_table_info_get_rows(const MonoTableInfo *table);
extern MonoClass *mono_class_get(MonoImage *image, uint32_t token);
extern const char *mono_class_get_name(MonoClass *klass);
extern const char *mono_class_get_namespace(MonoClass *klass);
extern MonoClass *mono_class_get_parent(MonoClass *klass);
extern MonoProperty *mono_class_get_properties(MonoClass *klass, void **iter);
extern const char *mono_property_get_name(MonoProperty *property);
extern MonoMethodSignature *mono_method_signature(MonoMethod *method);
extern uint32_t mono_signature_get_param_count(MonoMethodSignature *signature);

/*
 * Managed PUI value type ABI: the UIColor argument is passed by address to a
 * Mono AOT thunk. A by-value native struct would use the wrong registers and
 * can crash ShellUI.
 */
typedef struct RainbowUIColor {
    float r;
    float g;
    float b;
    float a;
} RainbowUIColor;

typedef void (*PuiTextColorAotFn)(
    void *label_instance,
    const RainbowUIColor *color
);

typedef struct RainbowRuntimeEvidence {
    MonoDomain *domain;
    MonoThread *attached_thread;
    MonoImage *pui_image;
    MonoClass *label_class;
    MonoMethod *text_color_setter;
    MonoMethod *application_update;
    PuiTextColorAotFn text_color_aot;
    void *application_update_aot;
} RainbowRuntimeEvidence;

static int g_log_fd = -1;
static size_t g_evidence_bytes = 0;
static bool g_evidence_truncated = false;
static bool g_evidence_write_failed = false;

typedef struct Sha256Context {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t block[64];
    size_t block_size;
} Sha256Context;

static Sha256Context g_evidence_hash;
static bool g_hash_evidence = false;

static uint32_t rotr32(uint32_t value, uint32_t shift) {
    return (value >> shift) | (value << (32U - shift));
}

static void sha256_transform(Sha256Context *ctx, const uint8_t block[64]) {
    static const uint32_t k[64] = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
        0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
        0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
        0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
        0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
        0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
        0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
        0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
        0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
        0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
        0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
        0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
    };
    uint32_t w[64];
    for (size_t i = 0; i < 16; ++i) {
        size_t at = i * 4;
        w[i] = ((uint32_t)block[at] << 24) |
               ((uint32_t)block[at + 1] << 16) |
               ((uint32_t)block[at + 2] << 8) |
               (uint32_t)block[at + 3];
    }
    for (size_t i = 16; i < 64; ++i) {
        uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^
                      (w[i - 15] >> 3);
        uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^
                      (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];
    uint32_t f = ctx->state[5];
    uint32_t g = ctx->state[6];
    uint32_t h = ctx->state[7];
    for (size_t i = 0; i < 64; ++i) {
        uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t choose = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + choose + k[i] + w[i];
        uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(Sha256Context *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->state[0] = 0x6a09e667U;
    ctx->state[1] = 0xbb67ae85U;
    ctx->state[2] = 0x3c6ef372U;
    ctx->state[3] = 0xa54ff53aU;
    ctx->state[4] = 0x510e527fU;
    ctx->state[5] = 0x9b05688cU;
    ctx->state[6] = 0x1f83d9abU;
    ctx->state[7] = 0x5be0cd19U;
}

static void sha256_update(Sha256Context *ctx, const void *data_ptr, size_t length) {
    const uint8_t *data = (const uint8_t *)data_ptr;
    for (size_t i = 0; i < length; ++i) {
        ctx->block[ctx->block_size++] = data[i];
        if (ctx->block_size == sizeof(ctx->block)) {
            sha256_transform(ctx, ctx->block);
            ctx->bit_count += 512U;
            ctx->block_size = 0;
        }
    }
}

static void sha256_final(Sha256Context *ctx, uint8_t digest[32]) {
    ctx->bit_count += (uint64_t)ctx->block_size * 8U;
    ctx->block[ctx->block_size++] = 0x80U;
    if (ctx->block_size > 56U) {
        while (ctx->block_size < 64U) {
            ctx->block[ctx->block_size++] = 0;
        }
        sha256_transform(ctx, ctx->block);
        ctx->block_size = 0;
    }
    while (ctx->block_size < 56U) {
        ctx->block[ctx->block_size++] = 0;
    }
    for (size_t i = 0; i < 8; ++i) {
        ctx->block[63U - i] = (uint8_t)(ctx->bit_count >> (i * 8U));
    }
    sha256_transform(ctx, ctx->block);
    for (size_t i = 0; i < 8; ++i) {
        digest[i * 4] = (uint8_t)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)ctx->state[i];
    }
}

static bool write_bytes(const void *data, size_t length, bool enforce_limit) {
    if (g_log_fd < 0 || g_evidence_write_failed) {
        return false;
    }
    if (enforce_limit && g_evidence_bytes + length > EVIDENCE_MAX_BYTES) {
        g_evidence_truncated = true;
        return false;
    }
    size_t offset = 0;
    while (offset < length) {
        size_t written = sceKernelWrite(
            g_log_fd,
            (const uint8_t *)data + offset,
            length - offset
        );
        if (written == 0 || written == (size_t)-1) {
            g_evidence_write_failed = true;
            return false;
        }
        offset += written;
    }
    if (g_hash_evidence) {
        sha256_update(&g_evidence_hash, data, length);
    }
    g_evidence_bytes += length;
    return true;
}

static void log_line(const char *format, ...) {
    if (g_log_fd < 0) {
        return;
    }

    char line[512];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    if (length <= 0) {
        return;
    }
    if ((size_t)length >= sizeof(line) - 1) {
        length = (int)sizeof(line) - 2;
    }
    line[length++] = '\n';
    write_bytes(line, (size_t)length, true);
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

static int ascii_lower(int value) {
    return (value >= 'A' && value <= 'Z') ? value + ('a' - 'A') : value;
}

static bool contains_case_insensitive(const char *text, const char *needle) {
    if (!text || !needle || !needle[0]) {
        return false;
    }
    for (const char *start = text; *start; ++start) {
        const char *left = start;
        const char *right = needle;
        while (*left && *right && ascii_lower((unsigned char)*left) ==
               ascii_lower((unsigned char)*right)) {
            ++left;
            ++right;
        }
        if (!*right) {
            return true;
        }
    }
    return false;
}

static bool class_is_relevant(const char *name_space, const char *name) {
    static const char *terms[] = {
        "Application", "Label", "Widget", "Scene", "Layer", "Area",
        "Root", "Theme", "Node", "Container", "View", "Element",
        "Frame", "Panel", "Color", "Text", "Focus", "Page", "Menu",
    };
    for (size_t i = 0; i < sizeof(terms) / sizeof(terms[0]); ++i) {
        if (contains_case_insensitive(name, terms[i]) ||
            contains_case_insensitive(name_space, terms[i])) {
            return true;
        }
    }
    return false;
}

static void dump_class_metadata(const char *image_label, MonoClass *klass) {
    const char *name = mono_class_get_name(klass);
    const char *name_space = mono_class_get_namespace(klass);
    MonoClass *parent = mono_class_get_parent(klass);
    const char *parent_name = parent ? mono_class_get_name(parent) : "";
    const char *parent_namespace = parent ? mono_class_get_namespace(parent) : "";
    log_line(
        "CLASS image=%s namespace=%s name=%s parent=%s.%s",
        image_label,
        name_space ? name_space : "",
        name ? name : "",
        parent_namespace ? parent_namespace : "",
        parent_name ? parent_name : ""
    );

    void *property_iter = NULL;
    MonoProperty *property = NULL;
    uint32_t property_count = 0;
    while (!g_evidence_truncated && property_count < MAX_PROPERTIES_PER_CLASS &&
           (property = mono_class_get_properties(klass, &property_iter)) != NULL) {
        const char *property_name = mono_property_get_name(property);
        MonoMethod *getter = mono_property_get_get_method(property);
        MonoMethod *setter = mono_property_get_set_method(property);
        log_line(
            " PROPERTY name=%s getter=%u setter=%u",
            property_name ? property_name : "",
            getter ? 1U : 0U,
            setter ? 1U : 0U
        );
        ++property_count;
    }
    if (property != NULL) {
        log_line(" PROPERTY_LIMIT count=%u", property_count);
    }

    void *method_iter = NULL;
    MonoMethod *method = NULL;
    uint32_t method_count = 0;
    while (!g_evidence_truncated && method_count < MAX_METHODS_PER_CLASS &&
           (method = mono_class_get_methods(klass, &method_iter)) != NULL) {
        const char *method_name = mono_method_get_name(method);
        MonoMethodSignature *signature = mono_method_signature(method);
        uint32_t parameter_count = signature
            ? mono_signature_get_param_count(signature)
            : UINT32_MAX;
        log_line(
            " METHOD name=%s params=%u",
            method_name ? method_name : "",
            parameter_count
        );
        ++method_count;
    }
    if (method != NULL) {
        log_line(" METHOD_LIMIT count=%u", method_count);
    }
    log_line("END_CLASS properties=%u methods=%u", property_count, method_count);
}

static int dump_image_metadata(const char *label, MonoImage *image) {
    if (!image) {
        log_line("IMAGE label=%s status=missing", label);
        return -1;
    }
    const MonoTableInfo *type_table = mono_image_get_table_info(
        image,
        MONO_TABLE_TYPEDEF
    );
    if (!type_table) {
        log_line("IMAGE label=%s status=no-typedef-table", label);
        return -2;
    }
    int rows = mono_table_info_get_rows(type_table);
    if (rows <= 0) {
        log_line("IMAGE label=%s status=empty rows=%d", label, rows);
        return -3;
    }

    uint32_t relevant = 0;
    log_line("IMAGE label=%s status=scanning typedef_rows=%d", label, rows);
    for (int row = 1; row <= rows && !g_evidence_truncated; ++row) {
        MonoClass *klass = mono_class_get(
            image,
            MONO_TOKEN_TYPE_DEF | (uint32_t)row
        );
        if (!klass) {
            continue;
        }
        const char *name = mono_class_get_name(klass);
        const char *name_space = mono_class_get_namespace(klass);
        if (!class_is_relevant(name_space, name)) {
            continue;
        }
        dump_class_metadata(label, klass);
        ++relevant;
        if (relevant >= MAX_RELEVANT_CLASSES) {
            log_line("CLASS_LIMIT image=%s count=%u", label, relevant);
            break;
        }
    }
    log_line(
        "END_IMAGE label=%s relevant_classes=%u truncated=%u",
        label,
        relevant,
        g_evidence_truncated ? 1U : 0U
    );
    return 0;
}

static MonoImage *open_image_candidates(
    MonoDomain *domain,
    const char *label,
    const char *const *paths,
    size_t path_count
) {
    for (size_t i = 0; i < path_count; ++i) {
        MonoAssembly *assembly = mono_domain_assembly_open(domain, paths[i]);
        if (!assembly) {
            continue;
        }
        MonoImage *image = mono_assembly_get_image(assembly);
        if (image) {
            log_line("ASSEMBLY label=%s path=%s status=opened", label, paths[i]);
            return image;
        }
    }
    log_line("ASSEMBLY label=%s status=not-found", label);
    return NULL;
}

static MonoImage *open_pui_image(MonoDomain *domain) {
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
        if (!assembly) {
            continue;
        }
        MonoImage *image = mono_assembly_get_image(assembly);
        if (image) {
            log_line("PUI image opened: %s", path);
            return image;
        }
    }
    return NULL;
}

static MonoImage *open_shellui_app_image(MonoDomain *domain) {
    static const char *paths[] = {
        "/app0/psm/Application/app.exe",
        "/app0/psm/Application/app.exe.sprx",
    };
    return open_image_candidates(
        domain,
        "SceShellUI-app",
        paths,
        sizeof(paths) / sizeof(paths[0])
    );
}

static void finish_evidence_file(int result) {
    uint8_t digest[32];
    char hex[65];
    Sha256Context final_hash = g_evidence_hash;
    sha256_final(&final_hash, digest);
    for (size_t i = 0; i < sizeof(digest); ++i) {
        static const char digits[] = "0123456789abcdef";
        hex[i * 2] = digits[digest[i] >> 4];
        hex[i * 2 + 1] = digits[digest[i] & 0x0fU];
    }
    hex[64] = '\0';

    g_hash_evidence = false;
    char footer[320];
    int length = snprintf(
        footer,
        sizeof(footer),
        "CONTENT_BYTES=%zu\nCONTENT_SHA256=%s\nTRUNCATED=%u\nWRITE_FAILED=%u\n"
        "RESULT=%d\nSTATUS=%s\n",
        g_evidence_bytes,
        hex,
        g_evidence_truncated ? 1U : 0U,
        g_evidence_write_failed ? 1U : 0U,
        result,
        (result == 0 && !g_evidence_truncated && !g_evidence_write_failed)
            ? "complete"
            : "incomplete"
    );
    if (length > 0) {
        size_t footer_size = (size_t)length < sizeof(footer)
            ? (size_t)length
            : sizeof(footer) - 1U;
        /* The footer is always attempted even when the bounded evidence body
         * reached its cap; STATUS then remains explicitly incomplete. */
        write_bytes(footer, footer_size, false);
    }
    if (g_log_fd >= 0) {
        sceKernelFsync(g_log_fd);
    }
}

static int dump_metadata_evidence(const RainbowRuntimeEvidence *evidence) {
    if (!evidence || !evidence->domain || !evidence->pui_image) {
        log_line("COLLECTOR status=invalid-runtime-evidence");
        return -20;
    }

    log_line("COLLECTOR mode=read-only-mono-metadata version=1");
    log_line("COLLECTOR no_hooks=1 no_text_writes=1 no_ui_patches=1");
    int pui_result = dump_image_metadata("PUI", evidence->pui_image);
    MonoImage *app_image = open_shellui_app_image(evidence->domain);
    int app_result = dump_image_metadata("SceShellUI-app", app_image);
    if (pui_result != 0 || app_result != 0) {
        log_line(
            "COLLECTOR status=failed pui_result=%d app_result=%d",
            pui_result,
            app_result
        );
        return -21;
    }
    if (g_evidence_truncated || g_evidence_write_failed) {
        return -22;
    }
    log_line("COLLECTOR status=complete");
    return 0;
}

static int collect_runtime_evidence(RainbowRuntimeEvidence *out) {
    memset(out, 0, sizeof(*out));

    out->domain = mono_get_root_domain();
    if (!out->domain) {
        log_line("Mono root domain is unavailable");
        return -1;
    }

    out->attached_thread = mono_thread_attach(out->domain);
    if (!out->attached_thread) {
        log_line("mono_thread_attach failed");
        return -2;
    }

    out->pui_image = open_pui_image(out->domain);
    if (!out->pui_image) {
        log_line("PUI image was not found");
        return -3;
    }

#if XXF_RAINBOW_COLLECTOR
    /* Collector mode only needs the live domain plus PUI image. It must not
     * depend on, resolve, invoke, or write through the later effect members. */
    log_line("Collector attached to the live Mono domain and PUI image");
    return 0;
#else

    out->label_class = mono_class_from_name(
        out->pui_image,
        "Sce.PlayStation.PUI.UI2",
        "Label"
    );
    MonoClass *application_class = mono_class_from_name(
        out->pui_image,
        "Sce.PlayStation.PUI",
        "Application"
    );
    if (!out->label_class || !application_class) {
        log_line("Required PUI classes are absent");
        return -4;
    }

    MonoProperty *text_color = mono_class_get_property_from_name(
        out->label_class,
        "TextColor"
    );
    out->text_color_setter = text_color
        ? mono_property_get_set_method(text_color)
        : NULL;
    out->application_update = mono_class_get_method_from_name(
        application_class,
        "Update",
        0
    );
    if (!out->text_color_setter || !out->application_update) {
        log_line("TextColor setter or Application.Update is absent");
        return -5;
    }

    out->text_color_aot = (PuiTextColorAotFn)mono_aot_get_method(
        out->domain,
        out->text_color_setter
    );
    out->application_update_aot = mono_aot_get_method(
        out->domain,
        out->application_update
    );
    if (!out->text_color_aot || !out->application_update_aot) {
        log_line("PUI AOT addresses were not resolved");
        return -6;
    }

    log_line(
        "Validated PUI members: TextColor=%p Application.Update=%p",
        (void *)out->text_color_aot,
        out->application_update_aot
    );
    return 0;
#endif
}

static int activate_rainbow(const RainbowRuntimeEvidence *evidence) {
#if XXF_RAINBOW_COLLECTOR
    /* Collector mode deliberately returns from collect_runtime_evidence()
     * before resolving any setter. Validate only the fields the read-only
     * collector actually requires, otherwise every collector run stops here
     * with -1 and never writes its metadata body. */
    if (!evidence || !evidence->domain || !evidence->pui_image) {
        return -1;
    }
    return dump_metadata_evidence(evidence);
#else
    if (!evidence || !evidence->text_color_setter) {
        return -1;
    }

#if XXF_RAINBOW_METADATA_READY
    /*
     * Implementation point: enumerate the exact live 14.02 ShellUI tree,
     * schedule a 20 Hz HSV tick on ShellUI's own update thread, and call
     * mono_runtime_invoke(text_color_setter, label, &args, &exception).
     * This branch is enabled only after the captured metadata proves the
     * root/child APIs. No setter or Update prologue is detoured here.
     */
    return -1;
#else
    log_line("Rainbow activation held: exact 14.02 label-tree metadata pending");
    return RAINBOW_PENDING_METADATA;
#endif
#endif
}

static void *initialise_thread(void *unused) {
    (void)unused;
    sceKernelUsleep(1000000);

    RainbowRuntimeEvidence evidence;
    int result = collect_runtime_evidence(&evidence);
    if (result == 0) {
        result = activate_rainbow(&evidence);
    }
    if (evidence.attached_thread) {
        mono_thread_detach(evidence.attached_thread);
        evidence.attached_thread = NULL;
    }
    log_line("Initialisation result: %d", result);
    finish_evidence_file(result);
    if (g_log_fd >= 0) {
        sceKernelClose(g_log_fd);
        g_log_fd = -1;
    }
#if XXF_RAINBOW_COLLECTOR
    if (result == 0) {
        notify_user("XXFWilson23: ShellUI metadata saved to /user/data");
    } else {
        notify_user("XXFWilson23: metadata collector failed (%d)", result);
    }
#elif XXF_RAINBOW_METADATA_READY
    if (result == 0) {
        notify_user("XXFWilson23: rainbow ShellUI text enabled");
    } else {
        notify_user("XXFWilson23: rainbow module failed (%d)", result);
    }
#else
    notify_user("XXFWilson23: rainbow metadata gate pending");
#endif
    return NULL;
}

int module_start(size_t argc, const void *args) {
    (void)argc;
    (void)args;

    sceKernelMkdir(EVIDENCE_DIR, 0777);
    g_log_fd = sceKernelOpen(LOG_PATH, 0x0601, 0666);
    if (g_log_fd < 0) {
        notify_user("XXFWilson23: metadata evidence file could not be opened");
        return g_log_fd;
    }
    g_evidence_bytes = 0;
    g_evidence_truncated = false;
    g_evidence_write_failed = false;
    sha256_init(&g_evidence_hash);
    g_hash_evidence = true;
    log_line("XXFWilson23 ShellUI module loaded");
    log_line("EVIDENCE_PATH=%s", LOG_PATH);

    OrbisPthread thread;
    int result = scePthreadCreate(
        &thread,
        NULL,
        initialise_thread,
        NULL,
        "xxf_rainbow_init"
    );
    if (result != 0) {
        log_line("Initialisation thread creation failed: 0x%08x", result);
        finish_evidence_file(result);
        sceKernelClose(g_log_fd);
        g_log_fd = -1;
        notify_user("XXFWilson23: collector thread failed (0x%08x)", result);
        return result;
    }
    scePthreadDetach(thread);
    return 0;
}

int module_stop(size_t argc, const void *args) {
    (void)argc;
    (void)args;
    log_line("XXFWilson23 ShellUI module stopped");
    if (g_log_fd >= 0) {
        sceKernelClose(g_log_fd);
        g_log_fd = -1;
    }
    return 0;
}
