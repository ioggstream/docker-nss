/*
 * NSS plugin for looking up Docker containers
 *
 * (c) 2014  Danielle Madeley  <danielle@madeley.id.au>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.

 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <glib.h>
#include <nss.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>

/* define a suffix that containers have */
#define SUFFIX ".docker"

#define ALIGN(a) (((a+sizeof(void*)-1)/sizeof(void*))*sizeof(void*))

static void
pack_hostent(struct hostent *result,
        char *buffer,
        size_t buflen,
        const char *name,
        const void *addr)
{
    char *aliases, *r_addr, *addrlist;
    size_t l, idx;

    /* we can't allocate any memory, the buffer is where we need to
     * return things we want to use
     *
     * 1st, the hostname */
    l = strlen(name);
    result->h_name = buffer;
    memcpy (result->h_name, name, l);
    buffer[l] = '\0';

    idx = ALIGN (l+1);

    /* 2nd, the empty aliases array */
    aliases = buffer + idx;
    *(char **) aliases = NULL;
    idx += sizeof (char*);

    result->h_aliases = (char **) aliases;

    result->h_addrtype = AF_INET;
    result->h_length = sizeof (struct in_addr);

    /* 3rd, address */
    r_addr = buffer + idx;
    memcpy(r_addr, addr, result->h_length);
    idx += ALIGN (result->h_length);

    /* 4th, the addresses ptr array */
    addrlist = buffer + idx;
    ((char **) addrlist)[0] = r_addr;
    ((char **) addrlist)[1] = NULL;

    result->h_addr_list = (char **) addrlist;
}

static gboolean
lookup_container_ip (const char *name,
        struct in_addr *addr)
{
    char *stdout_s, *name_s;
    int exit_status;
    gboolean success = FALSE;

    /* remove the suffix */
    name_s = g_strdup (name);
    *strrchr(name_s, '.') = '\0';

    char *argv[] = {
        "docker",
        "inspect",
        "-format={{.NetworkSettings.IPAddress}}",
        name_s,
        NULL,
    };

    if (!g_spawn_sync(NULL,
               argv,
               NULL,
               G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_SEARCH_PATH,
               NULL, NULL,
               &stdout_s, NULL,
               &exit_status,
               NULL))
      {
          goto finally;
      }

    if (exit_status != 0)
      {
        goto finally;
      }

    stdout_s = g_strstrip (stdout_s);
    success = inet_aton (stdout_s, addr);

finally:

    g_free (name_s);
    g_free (stdout_s);

    return success;
}

enum nss_status
_nss_docker_gethostbyname2_r (const char *name,
        int af,
        struct hostent *result,
        char *buffer,
        size_t buflen,
        int *errnop,
        int *h_errnop)
{
    struct in_addr addr;

    if (af != AF_INET)
      {
        *errnop = EAFNOSUPPORT;
        *h_errnop = NO_DATA;
        return NSS_STATUS_UNAVAIL;
      }

    if (!g_str_has_suffix(name, SUFFIX))
      {
        *errnop = ENOENT;
        *h_errnop = HOST_NOT_FOUND;
        return NSS_STATUS_NOTFOUND;
      }

    if (!lookup_container_ip (name, &addr))
      {
        *errnop = ENOENT;
        *h_errnop = HOST_NOT_FOUND;
        return NSS_STATUS_NOTFOUND;
      }

    pack_hostent(result, buffer, buflen, name, &addr);

    return NSS_STATUS_SUCCESS;
}

enum nss_status
_nss_docker_gethostbyname_r (const char *name,
        struct hostent *result,
        char *buffer,
        size_t buflen,
        int *errnop,
        int *h_errnop)
{
    return _nss_docker_gethostbyname2_r(name,
            AF_INET,
            result,
            buffer,
            buflen,
            errnop,
            h_errnop);
}

enum nss_status
_nss_docker_gethostbyaddr_r (const void *addr,
        socklen_t len,
        int af,
        struct hostent *result,
        char *buffer,
        size_t buflen,
        int *errnop,
        int *h_errnop)
{
    if (af != AF_INET)
      {
        *errnop = EAFNOSUPPORT;
        *h_errnop = NO_DATA;
        return NSS_STATUS_UNAVAIL;
      }

    if (len != sizeof (struct in_addr))
      {
        *errnop = EINVAL;
        *h_errnop = NO_RECOVERY;
        return NSS_STATUS_UNAVAIL;
      }

    /* FIXME: need to implement reverse lookups */
    *errnop = EAFNOSUPPORT;
    *h_errnop = NO_DATA;
    return NSS_STATUS_UNAVAIL;
    /*
    pack_hostent(result, buffer, buflen, "container.docker", addr);

    return NSS_STATUS_SUCCESS;
    */
}


/**
 * parse mappings to docker
8080/tcp.49156 9999/tcp.49157
* @return: TRUE on success
*/
static
gboolean _find_port(int *mapped, const char *output, const char *port) {
        // XXX check port value
        char *token= NULL, *tmp;
        char *strd = strdup(output);
        char *needle = calloc(11, sizeof(char));
        int l = snprintf(needle, 10, "%s/tcp.", port);
        int ret = FALSE;
        for (token = strtok_r(strd, " ", &tmp); token; token=strtok_r(NULL, " ", &tmp)) {
                //printf("needle:%s haystack:%s\n", needle, token);
                if (!strncmp(token, needle, l)){
                        *mapped = atoi(token+l);
                        //printf("found port %d\n",*mapped);
                        ret = TRUE;
                        goto finally;
                }
        }
        
    finally:
        free(strd);
        free(needle);
        return ret;
}


/**
 * parse mappings to docker
8080/tcp.49156 9999/tcp.49157

* @param hport- the resulting port
* @param name - the name of the port eg. badget.8080
*/
static gboolean
lookup_container_port (int *hport, const char *name)
{
    char *stdout_s, *name_s;
    int exit_status;
    gboolean success = FALSE;

    /* remove the suffix XXX the check is done elsewhere, is that right? */
    name_s = g_strdup (name);
    int l = strlen(name_s);
    char *port = strrchr(name_s, '.');
    if (!port || (port - name_s) >= l - 1)
        return FALSE;
    *port = '\0';
    port++; // now p contains the port number

    char *argv[] = {
        "docker",
        "inspect",
        "--format={{range $p, $conf := .NetworkSettings.Ports}} {{$p}}.{{(index $conf 0).HostPort}}{{end}}",
        name_s,
        NULL,
    };

    if (!g_spawn_sync(NULL,
               argv,
               NULL,
               G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_SEARCH_PATH,
               NULL, NULL,
               &stdout_s, NULL,
               &exit_status,
               NULL))
      {
          goto finally;
      }

    if (exit_status != 0)
      {
        goto finally;
      }

    stdout_s = g_strstrip (stdout_s);
    success = _find_port(hport, stdout_s, port);

finally:

    g_free (name_s);
    g_free (stdout_s);

    return success;
}

/**
 * Pack the server entry data into struct servent + buffer
 *  as specified by man:getservbyname_r
 */
static void
pack_servent(struct servent *result,
        char *buffer,
        size_t buflen,
        const char *name,
        const int *port)
{
    /* Cleanup passed buffer. */
    if (!memset(buffer, 0, buflen)){
        perror("memset");
        exit(EXIT_FAILURE);
    }
    char *aliases;
    size_t l;

    /* we can't allocate any memory, the buffer is where we need to
     * return things we want to use
     *
     * 1st, the hostname */
    // printf("buflen: %d\n", (int)buflen);
    char *p = buffer;
    l = strlen(name);
    result->s_name = p;
    p = mempcpy (result->s_name, name, l);
    p += ALIGN (l+1);
    
    
    // the protocol
    // printf("proto: %ld\n", p-buffer);
    result->s_proto = p;
    p = memcpy(result->s_proto, "tcp\0", 4);
    
    /* 2nd, the empty aliases array */
    // printf("aliases:  %ld\n", p-buffer);
    aliases = p;
    *(char **) aliases = NULL;
    result->s_aliases = (char **) aliases;

    
    /* 3rd, port */
    result->s_port = htons(*port);

}


/**
 * Parses docker ports in the form of
 * dockername.nattedport to service entry
 */
enum nss_status
_nss_docker_getservbyname_r (const char *name,
        const char *proto,
        struct servent *result_buf,
        char *buf,
        size_t buflen,
        struct servent **result)
{
    int port=0;
    
    /* just use tcp
    if (!proto)
        return ENOENT;
    */

    /* ignore undotted names */
    if (!index(name, '.'))
      return ENOENT;

    if (!lookup_container_port (&port, name))
        return ENOENT;
        
        
    pack_servent(result_buf, buf, buflen, name, &port);
    
    return NSS_STATUS_SUCCESS;
}
