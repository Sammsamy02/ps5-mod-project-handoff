/* Exercise the actual C ABI, including size queries and guard bytes. */
#include "nexus_bridge.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    static const unsigned char games[] =
        "[{\"id\":444,\"domain_name\":\"newgame2028\",\"name\":\"Fixture Game\","
        "\"mods\":5,\"key\":\"private-fixture-secret\"}]";
    static const unsigned char mod[] =
        "{\"mod_id\":17,\"game_id\":444,\"domain_name\":\"newgame2028\","
        "\"name\":\"Fixture Mod\",\"author\":\"Fixture Author\","
        "\"available\":true,\"status\":\"published\",\"contains_adult_content\":false}";
    char output[4096];
    size_t length = 123;
    int status = mb_nexus_decode(MB_NEXUS_GAMES, games, sizeof(games)-1,
                                 NULL, 0, 0, NULL, 0, &length);
    assert(status == MB_NEXUS_BUFFER_TOO_SMALL && length > 0);
    size_t required = length;
    char *guard = malloc(required + 3);
    assert(guard != NULL);
    memset(guard, 'x', required + 3);
    status = mb_nexus_decode(MB_NEXUS_GAMES, games, sizeof(games)-1,
                            NULL, 0, 0, guard+1, required, &length);
    assert(status == MB_NEXUS_BUFFER_TOO_SMALL && length == required);
    assert(guard[0]=='x' && guard[1]==0 && guard[required+1]=='x' && guard[required+2]=='x');
    status = mb_nexus_decode(MB_NEXUS_GAMES, games, sizeof(games)-1,
                            NULL, 0, 0, guard+1, required+1, &length);
    assert(status == MB_NEXUS_OK && length == required && strlen(guard+1) == length);
    assert(guard[0]=='x' && guard[required+1]==0 && guard[required+2]=='x');
    assert(strstr(guard+1, "\"status\":\"decoded\"") != NULL);
    assert(strstr(guard+1, "https://www.nexusmods.com/newgame2028") != NULL);
    assert(strstr(guard+1, "private-fixture-secret") == NULL);
    assert(strstr(guard+1, "\"installation_supported\":false") != NULL);
    free(guard);

    memset(output, 'x', sizeof(output));
    assert(mb_nexus_decode(MB_NEXUS_GAMES, games, sizeof(games)-1, NULL, 0, 0,
                           output, sizeof(output), NULL)==MB_NEXUS_INVALID_ARGUMENT && !output[0]);
    assert(mb_nexus_decode(MB_NEXUS_GAMES, games, sizeof(games)-1, NULL, 0, 0,
                           NULL, 1, &length)==MB_NEXUS_INVALID_ARGUMENT && !length);
    assert(mb_nexus_decode(MB_NEXUS_GAMES, NULL, 1, NULL, 0, 0,
                           output, sizeof(output), &length)==MB_NEXUS_INVALID_ARGUMENT && !length && !output[0]);
    assert(mb_nexus_decode(99, games, sizeof(games)-1, NULL, 0, 0,
                           output, sizeof(output), &length)==MB_NEXUS_INVALID_ARGUMENT && !length && !output[0]);
    unsigned char one = 0;
    assert(mb_nexus_decode(MB_NEXUS_GAMES, &one, MB_NEXUS_INPUT_LIMIT_BYTES+1,
                           NULL, 0, 0, output, sizeof(output), &length)==MB_NEXUS_INPUT_LIMIT && !length && !output[0]);
    assert(mb_nexus_decode(MB_NEXUS_GAMES, games, sizeof(games)-1, NULL, 0, 0,
                           output, MB_NEXUS_OUTPUT_LIMIT_BYTES+1, &length)==MB_NEXUS_INVALID_ARGUMENT && !length && !output[0]);
    assert(mb_nexus_decode(MB_NEXUS_MOD, mod, sizeof(mod)-1, NULL, 5, 17,
                           output, sizeof(output), &length)==MB_NEXUS_INVALID_ARGUMENT && !length && !output[0]);
    assert(mb_nexus_decode(MB_NEXUS_MOD, mod, sizeof(mod)-1, "x", 81, 17,
                           output, sizeof(output), &length)==MB_NEXUS_INVALID_ARGUMENT && !length && !output[0]);
    assert(mb_nexus_decode(MB_NEXUS_MOD, mod, sizeof(mod)-1, "newgame2028", 11, 0,
                           output, sizeof(output), &length)==MB_NEXUS_INVALID_ARGUMENT && !length && !output[0]);
    assert(mb_nexus_decode(MB_NEXUS_MOD, mod, sizeof(mod)-1, "newgame2028", 11, UINT64_MAX,
                           output, sizeof(output), &length)==MB_NEXUS_INVALID_ARGUMENT && !length && !output[0]);
    assert(mb_nexus_decode(MB_NEXUS_GAMES, games, sizeof(games)-1, "newgame2028", 11, 0,
                           output, sizeof(output), &length)==MB_NEXUS_INVALID_ARGUMENT && !length && !output[0]);
    assert(mb_nexus_decode(MB_NEXUS_MODS, games, sizeof(games)-1, "newgame2028", 11, 17,
                           output, sizeof(output), &length)==MB_NEXUS_INVALID_ARGUMENT && !length && !output[0]);
    output[0]='x';
    assert(mb_nexus_decode(MB_NEXUS_GAMES, games, sizeof(games)-1, NULL, 0, 0,
                           output, 0, &length)==MB_NEXUS_BUFFER_TOO_SMALL && output[0]=='x');

    assert(mb_nexus_decode(MB_NEXUS_MOD, mod, sizeof(mod)-1, "newgame2028", 11, 17,
                           output, sizeof(output), &length)==MB_NEXUS_OK);
    assert(strstr(output, "\"status\":\"decoded\"") != NULL);
    assert(strstr(output, "Fixture Author") != NULL);
    assert(strstr(output, "https://www.nexusmods.com/newgame2028/mods/17") != NULL);
    assert(mb_nexus_decode(MB_NEXUS_MOD, mod, sizeof(mod)-1, "newgame2028", 11, 18,
                           output, sizeof(output), &length)==MB_NEXUS_OK);
    assert(strstr(output, "\"status\":\"malformed\"") != NULL);
    assert(strstr(output, "Fixture Author") == NULL);
    assert(mb_nexus_decode(MB_NEXUS_GAMES, NULL, 0, NULL, 0, 0,
                           output, sizeof(output), &length)==MB_NEXUS_OK);
    assert(strstr(output, "\"status\":\"malformed\"") != NULL);
    assert(strlen(output)==length);
    assert(mb_nexus_metadata_url(MB_NEXUS_FILES, "future_game-2", 13, 71,
                                 NULL, 0, &length)==MB_NEXUS_BUFFER_TOO_SMALL);
    required=length;
    assert(required > 0 && required < MB_NEXUS_URL_LIMIT_BYTES);
    memset(output, 'x', sizeof(output));
    assert(mb_nexus_metadata_url(MB_NEXUS_FILES, "future_game-2", 13, 71,
                                 output, required, &length)==MB_NEXUS_BUFFER_TOO_SMALL && !output[0] && length==required);
    assert(mb_nexus_metadata_url(MB_NEXUS_FILES, "future_game-2", 13, 71,
                                 output, required+1, &length)==MB_NEXUS_OK);
    assert(!strcmp(output, "https://api.nexusmods.com/v1/games/future_game-2/mods/71/files") && strlen(output)==length);
    assert(output[required+1]=='x');
    assert(mb_nexus_metadata_url(MB_NEXUS_MODS, "x/y", 3, 0,
                                 output, MB_NEXUS_URL_LIMIT_BYTES, &length)==MB_NEXUS_INVALID_ARGUMENT && !length && !output[0]);
    assert(mb_nexus_metadata_url(MB_NEXUS_MODS, NULL, 1, 0,
                                 output, MB_NEXUS_URL_LIMIT_BYTES, &length)==MB_NEXUS_INVALID_ARGUMENT);
    assert(mb_nexus_metadata_url(MB_NEXUS_GAMES, NULL, 0, 0,
                                 NULL, 1, &length)==MB_NEXUS_INVALID_ARGUMENT);
    assert(mb_nexus_metadata_url(MB_NEXUS_GAMES, NULL, 0, 0,
                                 output, MB_NEXUS_URL_LIMIT_BYTES+1, &length)==MB_NEXUS_INVALID_ARGUMENT);
    assert(mb_nexus_metadata_url(MB_NEXUS_GAMES, NULL, 0, 0,
                                 output, MB_NEXUS_URL_LIMIT_BYTES, NULL)==MB_NEXUS_INVALID_ARGUMENT && !output[0]);
    output[0]='x';
    assert(mb_nexus_metadata_url(MB_NEXUS_GAMES, NULL, 0, 0,
                                 output, 0, &length)==MB_NEXUS_BUFFER_TOO_SMALL && output[0]=='x');
    puts("Nexus C ABI checks passed: whole reports, bounds, context, secret omission, routing");
    return 0;
}
