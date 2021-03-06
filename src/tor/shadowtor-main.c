/*
 * The Shadow Simulator
 * Copyright (c) 2010-2011, Rob Jansen
 * See LICENSE for licensing information
 */

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <glib.h>
#include <gmodule.h>

#define OPENSSL_THREAD_DEFINES
#include <openssl/opensslconf.h>
#include <openssl/crypto.h>

extern int tor_main(int argc, char *argv[]);
extern void shadowtorpreload_init(GModule*, int);
extern void shadowtorpreload_clear(void);
extern const RAND_METHOD* RAND_get_rand_method();

typedef void (*CRYPTO_lock_func)(int, int, const char*, int);
typedef unsigned long (*CRYPTO_id_func)(void);

const char tor_git_revision[] =
#ifndef _MSC_VER
#include "micro-revision.i"
#endif
        "";

static char* _shadowtor_get_formatted_arg_str(char* arg_str,
        const char* homedir_str, const char* hostname_str) {
    char* found = NULL;
    GString* sbuffer = g_string_new(arg_str);

    /* replace first ~ with the home directory */
    if ((found = g_strstr_len(sbuffer->str, sbuffer->len, "~"))) {
        ssize_t position = (ssize_t) (found - sbuffer->str);
        sbuffer = g_string_erase(sbuffer, position, (ssize_t) 1);
        sbuffer = g_string_insert(sbuffer, position, homedir_str);
    }

    /* replace first ${NODEID} with the hostname */
    if ((found = g_strstr_len(sbuffer->str, sbuffer->len, "${NODEID}"))) {
        ssize_t position = (ssize_t) (found - sbuffer->str);
        sbuffer = g_string_erase(sbuffer, position, (ssize_t) 9);
        sbuffer = g_string_insert(sbuffer, position, hostname_str);
    }

    return g_string_free(sbuffer, FALSE);
}

static const char* _shadowtor_get_hostname_str() {
    char hostname_buf[128];
    memset(hostname_buf, 0, 128);
    if (gethostname(&hostname_buf, 127) < 0) {
        return NULL;
    }
    return strdup(hostname_buf);
}

static const char* _shadowtor_get_homedir_str() {
    return g_get_home_dir();
}

static int _shadowtor_run(int argc, char *argv[]) {
    int retval = -1;

    /* get some basic info about this node */
    const char* homedir_str = _shadowtor_get_homedir_str();
    const char* hostname_str = _shadowtor_get_hostname_str();

    if (homedir_str && hostname_str) {
        /* convert special formatted arguments, like expanding '~' and '${NODEID}' */
        char** formatted_args = calloc(argc, sizeof(char*));

        for (int i = 0; i < argc; i++) {
            formatted_args[i] = _shadowtor_get_formatted_arg_str(argv[i], homedir_str, hostname_str);
        }

        /* launch tor! */
        retval = tor_main(argc, formatted_args);

        for (int i = 0; i < argc; i++) {
            free(formatted_args[i]);
        }
        free(formatted_args);
    }

    /* cleanup before return */
    if (homedir_str) {
        free(homedir_str);
    }
    if (hostname_str) {
        free(hostname_str);
    }

    return retval;
}

int main(int argc, char *argv[]) {
    return _shadowtor_run(argc, argv);
}

/* called immediately after the plugin is loaded. shadow loads plugins once for
 * each worker thread. the GModule* is needed as a handle for g_module_symbol()
 * symbol lookups.
 * return NULL for success, or a string describing the error */
const gchar* g_module_check_init(GModule *module) {
    /* how many locks does openssl want */
    int nLocks = CRYPTO_num_locks();

    /* initialize the preload lib, and its openssl thread handling */
    shadowtorpreload_init(module, nLocks);

    /* make sure openssl uses Shadow's random sources and make crypto thread-safe
     * get function pointers through LD_PRELOAD */
    const RAND_METHOD* shadowtor_randomMethod = RAND_get_rand_method();
    CRYPTO_lock_func shadowtor_lockFunc = CRYPTO_get_locking_callback();
    CRYPTO_id_func shadowtor_idFunc = CRYPTO_get_id_callback();

    CRYPTO_set_locking_callback(shadowtor_lockFunc);
    CRYPTO_set_id_callback(shadowtor_idFunc);
    RAND_set_rand_method(shadowtor_randomMethod);

    /* make sure libevent is thread-safe (returns non-zero if error)
     * the preload library will intercept and properly handling ids, locks and conditions */
    int libev_error_code = evthread_use_pthreads();
    if(libev_error_code != 0) {
        fprintf(stderr, "Error %i initializing libevent threading: %s\n", libev_error_code, strerror(errno));
        return "Error initializing libevent threading";
    } else {
        /* success */
        return NULL;
    }
}

/* called immediately after the plugin is unloaded. shadow unloads plugins
 * once for each worker thread. */
void g_module_unload(GModule *module) {
    shadowtorpreload_clear();
}
