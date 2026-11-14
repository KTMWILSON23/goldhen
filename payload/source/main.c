/*
 * XXFWilson23 one-shot rainbow ShellUI BinLoader payload.
 *
 * The generated embedded_shellui_prx.c is produced from the PRX built in the
 * same workflow. The payload writes it to /user/data, which is shared across
 * the BinLoader and SceShellUI process sandboxes, and asks GoldHEN to load it
 * into SceShellUI. The file stays present after a reported
 * successful load because the command's lifetime/ownership contract does not
 * prove that the module bytes are consumed eagerly. Failed writes/loads are
 * cleaned up.
 */

#include <ps4.h>

#define GOLDHEN_SDK_CMD_VERSION 0
#define GOLDHEN_SDK_CMD_PROCESS_PRX_LOAD 6
#define SHELLUI_PRX_PATH "/user/data/xxfwilson23_rainbow_shellui.prx"
#define PRX_LOAD_RESULT_SENTINEL ((uint64_t)-1)

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

static int load_shellui_prx(
    const char *path,
    int *command_result,
    uint64_t *module_result
) {
    struct proc_prx_load request;
    memset(&request, 0, sizeof(request));
    strncpy(request.process_name, "SceShellUI", sizeof(request.process_name) - 1);
    strncpy(request.prx_path, path, sizeof(request.prx_path) - 1);
    request.result = PRX_LOAD_RESULT_SENTINEL;

    *command_result = goldhen_command(
        GOLDHEN_SDK_CMD_PROCESS_PRX_LOAD,
        &request
    );
    *module_result = request.result;
    if (*command_result < 0 ||
        request.result == PRX_LOAD_RESULT_SENTINEL ||
        (int64_t)request.result < 0) {
        return -1;
    }
    return 0;
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
    if (!embedded_shellui_prx_len) {
        printf_notification("XXFWilson23: embedded ShellUI module is empty");
        return -2;
    }

    unlink(SHELLUI_PRX_PATH);
    int descriptor = open(
        SHELLUI_PRX_PATH,
        O_WRONLY | O_CREAT | O_TRUNC,
        0777
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
    if (result == 0 && fchmod(descriptor, 0777) < 0) {
        result = -1;
    }
    int close_result = close(descriptor);
    if (result == 0 && close_result < 0) {
        result = -1;
    }
    if (result != 0) {
        unlink(SHELLUI_PRX_PATH);
        printf_notification("XXFWilson23: module write or permission setup failed");
        return -4;
    }

    int load_command_result = 0;
    uint64_t load_module_result = PRX_LOAD_RESULT_SENTINEL;
    result = load_shellui_prx(
        SHELLUI_PRX_PATH,
        &load_command_result,
        &load_module_result
    );
    if (result < 0) {
        unlink(SHELLUI_PRX_PATH);
        printf_notification(
            "XXFWilson23: PRX load failed sdk=%d cmd=%d res=0x%llx",
            sdk_version,
            load_command_result,
            (unsigned long long)load_module_result
        );
        return -5;
    }

    printf_notification("XXFWilson23: rainbow ShellUI module loaded");
    return 0;
}
