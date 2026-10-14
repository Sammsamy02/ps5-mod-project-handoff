#ifndef MODBRIDGE_NEXUS_REQUIREMENTS_QUERY_H
#define MODBRIDGE_NEXUS_REQUIREMENTS_QUERY_H

/* Fixed read-only query from the pinned official node-nexus-api requirements
 * contract. Only typed domain/mod variables can be supplied. The forward
 * connection counts are required to detect truncated discovery. */
#define MB_NEXUS_REQUIREMENTS_GRAPHQL_URL "https://api.nexusmods.com/v2/graphql"
#define MB_NEXUS_REQUIREMENTS_GRAPHQL_QUERY \
    "query ModBridgeRequirements($modId: Int!, $gameDomainName: String!) { " \
    "modRequirements(modId: $modId, gameDomainName: $gameDomainName) { " \
    "nexusRequirements { nodes { id externalRequirement gameId modId modName notes url } nodesCount totalCount } " \
    "dlcRequirements { gameExpansion { gameId id name } notes } } }"
#define MB_NEXUS_REQUIREMENTS_BODY_PREFIX \
    "{\"query\":\"" MB_NEXUS_REQUIREMENTS_GRAPHQL_QUERY "\",\"variables\":{\"gameDomainName\":\""
#define MB_NEXUS_REQUIREMENTS_BODY_MIDDLE "\",\"modId\":"
#define MB_NEXUS_REQUIREMENTS_BODY_SUFFIX "}}"
#define MB_NEXUS_REQUIREMENTS_REQUEST_LIMIT 4096u
#define MB_NEXUS_REQUIREMENTS_NODE_LIMIT 1024u

#endif
