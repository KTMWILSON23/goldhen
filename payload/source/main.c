/*
 * XXFWilson23 one-shot BinLoader payload.
 *
 * The generated embedded_shellui_prx.c is produced from the PRX built in the
 * same workflow. The payload writes it to a stable /data path and asks
 * GoldHEN to load it into SceShellUI. The file stays present after a reported
 * successful load because the command's lifetime/ownership contract does not
 * prove that the module bytes are consumed eagerly. Failed writes/loads are
 * cleaned up.
 */

#include <ps4.h>

#define GOLDHEN_SDK_CMD_VERSION 0
#define GOLDHEN_SDK_CMD_PROCESS_PRX_LOAD 6
#define SHELLUI_PRX_PATH "/data/xxfwilson23_rainbow_shellui.prx"

extern const unsigned char embedded_shellui_prx[];
extern const size_t embedded_shellui_prx_len;

struct proc_prx_load {
    char process_name[32];
    char prx_path[100];
    uint64_t result;
} __attribute__((packed));

static int goldhen_command(uint64_t command, void *data) {
    return (int)syscall(500, command, data);
}

static int load_shellui_prx(const char *path) {
    struct proc_prx_load request;
    memset(&request, 0, sizeof(request));
    strncpy(request.process_name, "SceShellUI", sizeof(request.process_name) - 1);
    strncpy(request.prx_path, path, sizeof(request.prx_path) - 1);

    int command_result = goldhen_command(
        GOLDHEN_SDK_CMD_PROCESS_PRX_LOAD,
        &request
    );
    if (command_result < 0) {
        return command_result;
    }
    return (int)request.result;
}

static int write_all(int descriptor, const unsigned char *data, size_t length) {
    size_t offset = 0;
    while (offset < length) {
        ssize_t written = write(descriptor, data + offset, length - offset);
        if (written <= 0) {
            return -1;
        }
        offset += (size_t)written;
    }
    return 0;
}

int _main(struct thread *thread) {
    (void)thread;
    initKernel();
    initLibc();

    int sdk_version = goldhen_command(GOLDHEN_SDK_CMD_VERSION, NULL);
    if (sdk_version <= 0) {
        printf_notification("XXFWilson23: GoldHEN SDK is unavailable");
        return -1;
    }
    if (!embedded_shellui_prx_len) {
        printf_notification("XXFWilson23: embedded ShellUI module is empty");
        return -2;
    }

    unlink(SHELLUI_PRX_PATH);
    int descriptor = open(
        SHELLUI_PRX_PATH,
        O_WRONLY | O_CREAT | O_TRUNC,
        0644
    );
    if (descriptor < 0) {
        printf_notification("XXFWilson23: temporary module write failed");
        return -3;
    }

    int result = write_all(
        descriptor,
        embedded_shellui_prx,
        embedded_shellui_prx_len
    );
    int close_result = close(descriptor);
    if (result == 0 && close_result < 0) {
        result = -1;
    }
    if (result != 0) {
        unlink(SHELLUI_PRX_PATH);
        printf_notification("XXFWilson23: incomplete ShellUI module write");
        return -4;
    }

    result = load_shellui_prx(SHELLUI_PRX_PATH);
    if (result < 0) {
        unlink(SHELLUI_PRX_PATH);
        printf_notification(
            "XXFWilson23: ShellUI module load failed (%d)",
            result
        );
        return result;
    }

#if XXF_RAINBOW_COLLECTOR
    printf_notification("XXFWilson23: ShellUI metadata collector loaded");
#elif XXF_RAINBOW_METADATA_READY
    printf_notification("XXFWilson23: rainbow ShellUI text enabled");
#else
    printf_notification("XXFWilson23: module loaded; metadata gate is pending");
#endif
    return 0;
}
