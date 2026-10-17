"""Mod search by name: the fixed GraphQL request, the strict decoder and the client. Owned fixtures only:
nothing here contacts Nexus."""
import json
from pathlib import Path
import re
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "companion"))
try:
    import modbridge_nexus as nexus
    import modbridge_nexus.search as search
except ModuleNotFoundError:
    nexus = search = None

GAME = "arbitrarygame"
# Typed out here, so a silent edit of the query constant fails (as nexus_tests.HAND_QUERY does for requirements).
HAND_QUERY = ("query ModBridgeSearch($gameDomainName: String!, $name: String!, $count: Int!, $offset: Int!) { "
              "mods(viewUploaderHidden: false, viewUserBlockedContent: false, "
              "filter: {op: AND, gameDomainName: [{value: $gameDomainName, op: EQUALS}], "
              "name: [{value: $name, op: WILDCARD}], status: [{value: \"published\", op: EQUALS}]}, "
              "sort: [{relevance: {direction: DESC}}, {downloads: {direction: DESC}}], "
              "offset: $offset, count: $count) { "
              "nodes { modId name summary version author adultContent status uploader { name } game { id domainName } } "
              "nodesCount totalCount } }")


class FixtureTransport:
    def __init__(self, responses):
        self.responses = iter(responses)
        self.requests = []

    def request(self, method, url, **kwargs):
        self.requests.append((method, url, kwargs))
        return next(self.responses)


def json_response(record, status=200, headers=None):
    return nexus.Response(status, headers or {}, json.dumps(record).encode())


def node(mod_id=17, **changes):
    value = {"modId": mod_id, "name": "Owned fixture", "summary": "Synthetic test data", "version": "1.2",
             "author": "Fixture author", "adultContent": False, "status": "published",
             "uploader": {"name": "Uploader"}, "game": {"id": 444, "domainName": GAME}}
    value.update(changes)
    return value


def reply(nodes, total=None, **changes):
    nodes = list(nodes)
    value = {"data": {"mods": {"nodes": nodes, "nodesCount": len(nodes), "totalCount": len(nodes) if total is None else total}}}
    value["data"]["mods"].update(changes)
    return value


def encode(value):
    return value if isinstance(value, bytes) else json.dumps(value, ensure_ascii=False).encode("utf-8")


def decode(value, domain=GAME, page=1):
    return search.decode_search(encode(value), domain, page)


def rest_mod(mod_id=17, **changes):
    """What /games/{game}/mods/latest_added carries for the same mod as node()."""
    value = {"mod_id": mod_id, "game_id": 444, "domain_name": GAME, "category_id": 3, "contains_adult_content": False,
             "name": "Owned fixture", "summary": "Synthetic test data", "description": "", "version": "1.2",
             "author": "Fixture author", "uploaded_by": "Uploader", "status": "published", "available": True}
    value.update(changes)
    return value


class SearchTest(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(nexus, "Nexus adapter has not been implemented")
        self.assertTrue(hasattr(nexus.NexusClient, "search_mods"), "search client not implemented")


class SearchRequestTests(SearchTest):
    def variables(self, body):
        return json.loads(body)["variables"]

    def test_the_query_is_one_fixed_constant_with_four_typed_variables(self):
        self.assertEqual(search.QUERY, HAND_QUERY)
        self.assertEqual(re.findall(r"\$(\w+): (\w+!)", search.QUERY.split("{", 1)[0]),
                         [("gameDomainName", "String!"), ("name", "String!"), ("count", "Int!"), ("offset", "Int!")])
        self.assertEqual(sorted(set(re.findall(r"\$(\w+)", search.QUERY))), ["count", "gameDomainName", "name", "offset"])
        for use in ("{value: $gameDomainName, op: EQUALS}", "{value: $name, op: WILDCARD}", "offset: $offset, count: $count"):
            self.assertEqual(search.QUERY.count(use), 1)
        # Read only: no mutation, no subscription, a single operation.
        self.assertTrue(search.QUERY.startswith("query "))
        self.assertNotIn("mutation", search.QUERY)
        self.assertEqual(search.QUERY.count("query "), 1)
        self.assertEqual((search.PAGE_SIZE, search.MAX_PAGE, search.TEXT_MIN, search.TEXT_MAX), (20, 50, 2, 100))

    def test_body_bytes_are_the_constant_query_plus_typed_variables(self):
        body = search.search_body("skyrim", "better ranger", 2)
        self.assertIsInstance(body, bytes)
        expected = ('{"query":' + json.dumps(search.QUERY) + ',"variables":{"gameDomainName":"skyrim",'
                    '"name":"better ranger","count":20,"offset":20}}').encode("ascii")
        self.assertEqual(body, expected)
        record = json.loads(body)
        self.assertEqual(set(record), {"query", "variables"})
        self.assertEqual(record["query"], search.QUERY)
        variables = record["variables"]
        self.assertEqual(list(variables), ["gameDomainName", "name", "count", "offset"])
        self.assertEqual([type(variables[key]) for key in variables], [str, str, int, int])
        self.assertLess(len(body), search.REQUEST_LIMIT)

    def test_offset_follows_the_page_and_count_is_fixed(self):
        for page, offset in ((1, 0), (2, 20), (3, 40), (50, 980)):
            with self.subTest(page=page):
                variables = self.variables(search.search_body(GAME, "ab", page))
                self.assertEqual((variables["count"], variables["offset"]), (20, offset))

    def test_typed_text_is_data_never_part_of_the_query(self):
        hostile = ['x"}) { viewer } #', "a\\b", '" } } mutation { x(y: "', "名前 \U0001f600 emoji", "$offset", "{{}}", "a​b",
                   "50% off", "tab\\tnewline\\n", "'; DROP", "</script>", "é́"]
        for text in hostile:
            with self.subTest(text=text):
                body = search.search_body(GAME, text, 1)
                self.assertTrue(body.isascii())
                record = json.loads(body)
                self.assertEqual(record["query"], search.QUERY)
                self.assertEqual(record["variables"]["name"], search.search_text(text))
                self.assertEqual(body.count(b'"query"'), 1)

    def test_text_is_trimmed_and_inner_space_runs_collapse(self):
        for raw, expected in (("  better   ranger ", "better ranger"), ("ab", "ab"), ("  ab　cd  ", "ab cd"),
                              ("x" * 100, "x" * 100), ("  " + "x" * 100 + "  ", "x" * 100), ("名前", "名前"),
                              ("\U0001f600" * 100, "\U0001f600" * 100)):
            with self.subTest(raw=raw[:20]):
                self.assertEqual(search.search_text(raw), expected)
        # One normalized form: the cache key.
        self.assertEqual(search.search_text("a   b"), search.search_text(" a b "))
        self.assertLess(len(search.search_body(GAME, "\U0001f600" * 100, 1)), search.REQUEST_LIMIT)

    def test_invalid_text_is_refused_before_anything_is_built(self):
        bad = [None, 5, True, b"ab", ["ab"], "", " ", "a", " a ", "  a  ", "x" * 101, " " + "x" * 101 + " ", "x" * 1025,
               "\x00ab", "a\x00", "a\tb", "a\nb", "ab\n", "\rab", "\x7fab", "a\x85b", "a\x1bb", "a b", "a b", "ab\ud800"]
        for text in bad:
            with self.subTest(text=repr(text)[:30]):
                with self.assertRaises(nexus.NexusError) as caught:
                    search.search_text(text)
                self.assertEqual(caught.exception.code, "invalid_input")
                with self.assertRaises(nexus.NexusError) as caught:
                    search.search_body(GAME, text, 1)
                self.assertEqual(caught.exception.code, "invalid_input")

    def test_invalid_page_and_game_are_refused(self):
        for page in (0, -1, 51, 1000, 2 ** 63, True, False, None, "1", 1.0, 2.5, [1]):
            with self.subTest(page=page):
                with self.assertRaises(nexus.NexusError) as caught:
                    search.search_page(page)
                self.assertEqual(caught.exception.code, "invalid_input")
                with self.assertRaises(nexus.NexusError):
                    search.search_body(GAME, "ab", page)
        self.assertEqual([search.search_page(page) for page in (1, 2, 49, 50)], [1, 2, 49, 50])
        for game in ("../../users", "NAME", "", None, 5, "a b", "-x"):
            with self.subTest(game=game):
                with self.assertRaises(nexus.NexusError):
                    search.search_body(game, "ab", 1)

    def test_pages_follow_the_total_and_stop_at_fifty(self):
        self.assertEqual([search.search_pages(total) for total in (0, 1, 20, 21, 40, 41, 999, 1000, 1001, 5000, 2 ** 31 - 1)],
                         [0, 1, 1, 2, 2, 3, 50, 50, 50, 50, 50])


class SearchDecoderTests(SearchTest):
    def failed(self, result, status="malformed"):
        self.assertEqual(result["status"], status)
        self.assertNotIn("nodes", result)
        self.assertNotIn("total", result)

    def test_a_valid_page_decodes_to_the_rest_record_inputs(self):
        result = decode(reply([node(17), node(18, adultContent=True, name="第二の作品")]))
        self.assertEqual(result["status"], "decoded")
        self.assertEqual(result["total"], 2)
        self.assertEqual(result["nodes"][0], {
            "mod_id": 17, "game_id": 444, "domain_name": GAME, "name": "Owned fixture", "author": "Fixture author",
            "summary": "Synthetic test data", "version": "1.2", "contains_adult_content": False, "available": True,
            "status": "published"})
        self.assertEqual((result["nodes"][1]["name"], result["nodes"][1]["contains_adult_content"]), ("第二の作品", True))

    def test_empty_results_and_pages_past_the_end_are_complete_answers(self):
        result = decode(reply([]))
        self.assertEqual((result["status"], result["nodes"], result["total"]), ("decoded", [], 0))
        # Page 2 of 25 holds the last five; page 3 holds none.
        result = decode(reply([node(i) for i in range(1, 6)], total=25), page=2)
        self.assertEqual((result["status"], len(result["nodes"])), ("decoded", 5))
        result = decode(reply([], total=25), page=3)
        self.assertEqual((result["status"], result["nodes"], result["total"]), ("decoded", [], 25))
        full = decode(reply([node(i) for i in range(1, 21)], total=2 ** 31 - 1), page=1)
        self.assertEqual((full["status"], len(full["nodes"]), full["total"]), ("decoded", 20, 2 ** 31 - 1))
        # Page 50 starts at offset 980: 20 of 1000 remain.
        self.assertEqual(decode(reply([node(i) for i in range(1, 21)], total=1000), page=50)["status"], "decoded")

    def test_graphql_errors_are_never_partial_data(self):
        good = reply([node()])
        for errors in ([{"message": "SecretMarker"}], [{}], [None], ["x"], [{"message": "x"}] * 5):
            with self.subTest(errors=str(errors)[:30]):
                self.failed(decode(dict(good, errors=errors)))
                self.failed(decode({"errors": errors}))
                self.failed(decode({"data": None, "errors": errors}))
        for errors in (None, {}, "none", 0, False):
            with self.subTest(errors=errors):
                self.failed(decode(dict(good, errors=errors)))
        self.assertEqual(decode(dict(good, errors=[]))["status"], "decoded")

    def test_shape_errors_are_malformed(self):
        for body in ({}, [], "x", 5, None, {"data": None}, {"data": []}, {"data": {}}, {"data": {"mods": None}},
                     {"data": {"mods": []}}, {"data": {"mods": {}}}, {"data": {"modRequirements": {}}}):
            with self.subTest(body=str(body)[:40]):
                self.failed(decode(body))
        for field in ("nodes", "nodesCount", "totalCount"):
            value = reply([node()])
            del value["data"]["mods"][field]
            with self.subTest(missing=field):
                self.failed(decode(value))
        for bad in (None, {}, "x", 5, True):
            value = reply([node()])
            value["data"]["mods"]["nodes"] = bad
            with self.subTest(nodes=bad):
                self.failed(decode(value))
        for field in ("nodesCount", "totalCount"):
            for bad in (None, "1", 1.0, 1.5, True, False, -1, [1], {"v": 1}):
                with self.subTest(field=field, bad=bad):
                    self.failed(decode(reply([node()], **{field: bad})))
        self.failed(decode(reply(["not an object"])))
        self.failed(decode(reply([None])))
        self.failed(decode(reply([[node()]])))

    def test_counts_must_describe_exactly_the_requested_page(self):
        nodes = [node(i) for i in range(1, 4)]
        self.assertEqual(decode(reply(nodes))["status"], "decoded")
        self.failed(decode(reply(nodes, nodesCount=2)))             # nodesCount disagrees with the nodes
        self.failed(decode(reply(nodes, nodesCount=4)))
        self.failed(decode(reply(nodes, total=2)))                  # fewer matches than nodes returned
        self.failed(decode(reply(nodes, total=4)))                  # a short page although more matched
        self.failed(decode(reply(nodes, total=100)))                # truncated: a full page was due
        self.failed(decode(reply([node(i) for i in range(1, 21)], total=19)))
        self.failed(decode(reply([node(i) for i in range(1, 21)], total=25), page=2))   # 5 were due, 20 came
        self.failed(decode(reply([], total=5)))                     # matches exist but the page is empty
        self.failed(decode(reply([], total=45), page=2))            # 20 were due
        self.failed(decode(reply(nodes, total=3), page=2))          # page 2 starts past the end: none are due

    def test_bounds(self):
        twenty = [node(i) for i in range(1, 21)]
        self.assertEqual(decode(reply(twenty))["status"], "decoded")
        self.failed(decode(reply(twenty + [node(21)])), "resource_limit")
        self.failed(decode(reply(twenty, nodesCount=21)), "resource_limit")
        self.failed(decode(reply([node(i) for i in range(1, 200)])), "resource_limit")
        self.failed(decode(reply([], total=2 ** 31)), "resource_limit")
        self.failed(decode(reply([], total=2 ** 63)), "resource_limit")
        self.failed(decode(reply([], nodesCount=10 ** 9)), "resource_limit")
        self.failed(search.decode_search(encode(reply([])), GAME, 0))
        self.failed(search.decode_search(encode(reply([])), GAME, 51))
        self.failed(search.decode_search(encode(reply([])), GAME, True))
        self.failed(search.decode_search(encode(reply([])), "NOT A DOMAIN", 1))

    def test_every_node_field_is_typed(self):
        for key in ("modId", "name", "summary", "version", "author", "adultContent", "status", "uploader", "game"):
            value = node()
            del value[key]
            with self.subTest(missing=key):
                self.failed(decode(reply([value])))
        for key, value in (("uploader", {}), ("uploader", None), ("uploader", "x"), ("uploader", {"name": None}),
                           ("uploader", {"name": 5}), ("game", {}), ("game", None), ("game", {"id": 444}),
                           ("game", {"domainName": GAME}), ("game", {"id": "444", "domainName": GAME}),
                           ("game", {"id": 0, "domainName": GAME}), ("game", {"id": True, "domainName": GAME}),
                           ("game", {"id": 2 ** 31, "domainName": GAME}), ("game", {"id": 444, "domainName": None}),
                           ("modId", "17"), ("modId", 17.0), ("modId", True), ("modId", 0), ("modId", -3),
                           ("modId", 2 ** 31), ("modId", None), ("name", None), ("name", 5), ("summary", None),
                           ("summary", 5), ("version", None), ("version", 1.2), ("author", 5), ("author", ["x"]),
                           ("adultContent", "false"), ("adultContent", 0), ("adultContent", 1), ("adultContent", "true"),
                           ("adultContent", []), ("status", None), ("status", 5), ("status", True)):
            with self.subTest(key=key, value=value):
                self.failed(decode(reply([node(**{key: value})])))

    def test_a_hit_for_another_game_is_never_returned(self):
        self.failed(decode(reply([node(game={"id": 444, "domainName": "othergame"})])))
        self.failed(decode(reply([node(1), node(2, game={"id": 9, "domainName": "othergame"})])))
        self.failed(decode(reply([node(game={"id": 444, "domainName": GAME.upper()})])))

    def test_duplicate_mods_in_a_page_are_refused(self):
        self.failed(decode(reply([node(17), node(18), node(17)])))

    def test_author_falls_back_to_the_uploader_and_unknown_adult_stays_unknown(self):
        for author in (None, ""):
            with self.subTest(author=author):
                row = decode(reply([node(author=author)]))["nodes"][0]
                self.assertEqual(row["author"], "Uploader")
        row = decode(reply([node(adultContent=None)]))["nodes"][0]
        self.assertIsNone(row["contains_adult_content"])

    def test_text_bounds_and_control_characters(self):
        for key, limit in (("name", 1024), ("author", 1024), ("version", 256), ("summary", 65536), ("status", 64)):
            with self.subTest(key=key):
                self.assertEqual(decode(reply([node(**{key: "界" * limit})]))["status"], "decoded")
                self.failed(decode(reply([node(**{key: "界" * (limit + 1)})])))
        for char in ("\x00", "\x01", "\x08", "\x0b", "\x0c", "\x0e", "\x1f"):
            with self.subTest(char=repr(char)):
                for key in ("name", "summary", "version", "author", "status"):
                    self.failed(decode(reply([node(**{key: "safe" + char + "unsafe"})])))
                self.failed(decode(reply([node(uploader={"name": "a" + char})])))
        # Line breaks and tabs are ordinary text in a summary, as in the REST records.
        self.assertEqual(decode(reply([node(summary="a\nb\tc\r")]))["status"], "decoded")

    def test_extra_provider_fields_are_ignored_and_not_returned(self):
        value = reply([node(downloads=5, endorsements=9, updatedAt="2026-01-01T00:00:00Z", apikey="must-not-leak",
                            uploader={"name": "Uploader", "memberId": 7, "email": "x@example.test"},
                            game={"id": 444, "domainName": GAME, "name": "Arbitrary"})])
        value["data"]["mods"]["facets"] = {"x": 1}
        value["extensions"] = {"cost": 5}
        result = decode(value)
        self.assertEqual(result["status"], "decoded")
        self.assertNotIn("must-not-leak", json.dumps(result))
        self.assertEqual(set(result["nodes"][0]), {"mod_id", "game_id", "domain_name", "name", "author", "summary", "version",
                                                    "contains_adult_content", "available", "status"})

    def test_the_parse_gate_applies(self):
        base = json.dumps(reply([node()]))
        for body in (b"", b"{", b"not json", b"\xff\xfe", base.encode()[:-1], (base + " x").encode(), b"{} {}", b"\x00" + base.encode()):
            with self.subTest(body=body[:20]):
                self.failed(search.decode_search(body, GAME, 1))
        self.assertEqual(search.decode_search(b"\xef\xbb\xbf" + base.encode(), GAME, 1)["status"], "decoded")
        for non_bytes in (base, None, 5, [base]):
            self.failed(search.decode_search(non_bytes, GAME, 1))
        # Repeated keys, non-JSON constants, odd number forms, overflow, bad escapes and unpaired surrogates.
        for body in ('{"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0,"totalCount":0}}}',
                     '{"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":NaN}}}',
                     '{"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":Infinity}}}',
                     '{"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":-Infinity}}}',
                     '{"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0.0}}}',
                     '{"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0e0}}}',
                     '{"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":1e999}}}',
                     '{"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":00}}}',
                     '{"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0}},"x":1e999}',
                     '{"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0}},"x":"\\ud800"}',
                     '{"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0}},"x":"\\u0001"}',
                     '{"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0}},"\\u0001":1}',
                     '{"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0}},"data":1}'):
            with self.subTest(body=body[:70]):
                self.failed(search.decode_search(body.encode(), GAME, 1))
        for body in (base.replace('"modId": 17', '"modId": 17, "modId": 18'), base.replace('"name": "Owned fixture"', '"name": "a", "name": "b"')):
            self.failed(search.decode_search(body.encode(), GAME, 1))
        self.assertEqual(search.decode_search(
            '{"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0}},"x":"\\n\\t\\r"}'.encode(), GAME, 1)["status"], "decoded")

    def test_depth_array_and_size_limits(self):
        base = json.dumps(reply([]))
        def nested(depth):
            return '{"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0}},"x":' + "[" * depth + "]" * depth + "}"
        self.assertEqual(search.decode_search(nested(60).encode(), GAME, 1)["status"], "decoded")
        self.failed(search.decode_search(nested(100).encode(), GAME, 1), "resource_limit")
        self.failed(search.decode_search(nested(100000).encode(), GAME, 1), "resource_limit")
        wide = '{"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0}},"x":' + json.dumps([0] * 10001) + "}"
        self.failed(search.decode_search(wide.encode(), GAME, 1), "resource_limit")
        padded = base.encode() + b" " * (8 * 1024 * 1024 - len(base))
        self.assertEqual(search.decode_search(padded, GAME, 1)["status"], "decoded")
        self.failed(search.decode_search(padded + b" ", GAME, 1), "resource_limit")

    def test_client_facing_form_raises_payload_without_provider_text(self):
        record = search.search_page_record(encode(reply([node()])), GAME, 1)
        self.assertEqual(set(record), {"nodes", "total"})
        self.assertEqual((len(record["nodes"]), record["total"]), (1, 1))
        for bad in (b"", b"{}", encode(reply([node(name=None)])), encode(dict(reply([node()]), errors=[{"message": "SecretMarker"}])),
                    encode(reply([node(i) for i in range(1, 22)])), encode(reply([node()], total=5))):
            with self.subTest(size=len(bad)):
                with self.assertRaises(nexus.NexusError) as caught:
                    search.search_page_record(bad, GAME, 1)
                self.assertEqual(caught.exception.code, "payload")
                self.assertNotIn("SecretMarker", repr(caught.exception) + str(caught.exception))


class SearchClientTests(SearchTest):
    OAUTH = nexus.Credentials(access_token="opaque-fixture", refresh_token="refresh") if nexus else None

    def client(self, responses, credentials=None):
        transport = FixtureTransport([item if isinstance(item, nexus.Response) else json_response(item) for item in responses])
        return nexus.NexusClient(credentials or nexus.Credentials(api_key="fixture-key"), transport), transport

    @staticmethod
    def userinfo(adult):
        preferences = {} if adult == "absent" else {"adult": adult}
        return {"sub": "42", "name": "OAuth fixture", "membership_roles": ["member"], "preferences": preferences}

    def test_search_posts_the_fixed_query_with_the_same_credentials_as_rest(self):
        client, transport = self.client([reply([node(17), node(18, name="Second")], total=2)])
        result = client.search_mods(GAME, "  owned   fixture ")
        self.assertEqual(len(transport.requests), 1)
        method, url, kwargs = transport.requests[0]
        self.assertEqual((method, url), ("POST", "https://api.nexusmods.com/v2/graphql"))
        self.assertEqual(kwargs["body"], search.search_body(GAME, "owned fixture", 1))
        self.assertEqual(json.loads(kwargs["body"])["variables"], {"gameDomainName": GAME, "name": "owned fixture", "count": 20, "offset": 0})
        self.assertEqual(kwargs["headers"], {"Accept": "application/json", "Application-Name": "PS5ModBridge",
            "Application-Version": "0.1.0", "apikey": "fixture-key", "Content-Type": "application/json"})
        self.assertEqual(set(kwargs), {"headers", "body"})
        self.assertEqual(set(result), {"items", "total", "page", "pages"})
        self.assertEqual((result["total"], result["page"], result["pages"]), (2, 1, 1))
        self.assertEqual([item["mod_id"] for item in result["items"]], [17, 18])
        oauth, transport = self.client([reply([node(i) for i in range(1, 6)], total=45)], self.OAUTH)
        self.assertEqual(oauth.search_mods(GAME, "ab", 3)["page"], 3)
        headers = transport.requests[0][2]["headers"]
        self.assertEqual((headers["Authorization"], headers["Content-Type"]), ("Bearer opaque-fixture", "application/json"))
        self.assertNotIn("apikey", headers)
        self.assertEqual(json.loads(transport.requests[0][2]["body"])["variables"]["offset"], 40)

    def test_page_defaults_to_one_and_pages_are_reported(self):
        for total, pages in ((0, 0), (1, 1), (20, 1), (21, 2), (999, 50), (1001, 50), (2 ** 31 - 1, 50)):
            with self.subTest(total=total):
                count = min(20, total)
                client, transport = self.client([reply([node(i) for i in range(1, count + 1)], total=total)])
                result = client.search_mods(GAME, "ab")
                self.assertEqual((result["page"], result["pages"], result["total"]), (1, pages, total))
                self.assertEqual(json.loads(transport.requests[0][2]["body"])["variables"]["offset"], 0)
        client, _ = self.client([reply([], total=25)])
        self.assertEqual(client.search_mods(GAME, "ab", 3), {"items": [], "total": 25, "page": 3, "pages": 2})
        client, _ = self.client([reply([], total=0)])
        self.assertEqual(client.search_mods(GAME, "ab"), {"items": [], "total": 0, "page": 1, "pages": 0})

    def test_invalid_input_is_refused_before_any_request(self):
        client, transport = self.client([])
        cases = [("../../users", "ab", 1), ("NAME", "ab", 1), ("", "ab", 1), (None, "ab", 1), (GAME, "a", 1), (GAME, " a ", 1),
                 (GAME, "", 1), (GAME, None, 1), (GAME, "x" * 101, 1), (GAME, "a\x00b", 1), (GAME, "a\nb", 1), (GAME, "ab", 0),
                 (GAME, "ab", 51), (GAME, "ab", True), (GAME, "ab", "1"), (GAME, "ab", None), (GAME, "ab", 1.0)]
        for game, text, page in cases:
            with self.subTest(game=game, text=repr(text)[:20], page=page):
                with self.assertRaises(nexus.NexusError) as caught:
                    client.search_mods(game, text, page)
                self.assertEqual(caught.exception.code, "invalid_input")
        self.assertEqual(transport.requests, [])

    def test_a_search_hit_is_the_same_record_a_latest_mod_is(self):
        rest, _ = self.client([[rest_mod(17), rest_mod(18, name="Second", author="Other", summary="", version="")]])
        latest = rest.latest_mods(GAME)
        client, _ = self.client([reply([node(17), node(18, name="Second", author="Other", summary="", version="")])])
        found = client.search_mods(GAME, "ab")["items"]
        self.assertEqual(found, latest)
        self.assertEqual([list(item) for item in found], [list(item) for item in latest])  # the same keys, in the same order
        self.assertEqual(found[0]["source_url"], "https://www.nexusmods.com/arbitrarygame/mods/17")
        self.assertEqual((found[0]["game_domain"], found[0]["game_id"], found[0]["description"]), (GAME, 444, ""))

    def test_content_rules_match_latest_mods_for_every_combination(self):
        # (adult, status) combinations in one page; REST and GraphQL data describe the same mods.
        combos = [(False, "published"), (True, "published"), (None, "published"), (False, "hidden"), (True, "hidden"),
                  (False, "under_moderation"), (False, "Published"), (False, "not_published"), (False, ""), (None, "hidden")]
        ids = list(range(100, 100 + len(combos)))
        rest_page = [rest_mod(i, contains_adult_content=adult, status=status) for i, (adult, status) in zip(ids, combos)]
        graph_page = [node(i, adultContent=adult, status=status) for i, (adult, status) in zip(ids, combos)]
        for label, credentials, info in (("api key", None, None), ("oauth adult", self.OAUTH, True), ("oauth not adult", self.OAUTH, False),
                                         ("oauth unknown", self.OAUTH, None), ("oauth absent", self.OAUTH, "absent"),
                                         ("oauth garbage", self.OAUTH, "true")):
            with self.subTest(label=label):
                rest_responses = [rest_page] + ([self.userinfo(info)] if info is not None or label == "oauth unknown" else [])
                graph_responses = [reply(graph_page)] + ([self.userinfo(info)] if info is not None or label == "oauth unknown" else [])
                rest, rest_transport = self.client(rest_responses, credentials)
                graph, graph_transport = self.client(graph_responses, credentials)
                latest = rest.latest_mods(GAME)
                found = graph.search_mods(GAME, "ab")
                self.assertEqual(found["items"], latest)
                self.assertEqual(found["total"], len(combos))
                # The same lookups: one listing, then at most one account-preference read, both times.
                self.assertEqual([request[1] for request in graph_transport.requests][1:],
                                 [request[1] for request in rest_transport.requests][1:])
                shown = [item["mod_id"] for item in found["items"]]
                expected = [100] if label in ("api key", "oauth not adult", "oauth unknown", "oauth absent", "oauth garbage") else [100, 101]
                self.assertEqual(shown, expected)
                for item in found["items"]:
                    if item["contains_adult_content"]:
                        self.assertEqual(label, "oauth adult")

    def test_adult_mods_never_appear_without_a_verified_preference(self):
        page = reply([node(1, adultContent=True), node(2, adultContent=True, name="Another"), node(3)])
        client, transport = self.client([page])
        self.assertEqual([item["mod_id"] for item in client.search_mods(GAME, "ab")["items"]], [3])
        self.assertEqual([request[0] for request in transport.requests], ["POST"])  # an API key has no preference to read
        for adult in (False, None, "absent", 1, "true", "yes"):
            with self.subTest(adult=adult):
                client, transport = self.client([page, self.userinfo(adult)], self.OAUTH)
                self.assertEqual([item["mod_id"] for item in client.search_mods(GAME, "ab")["items"]], [3])
                self.assertEqual([request[0] for request in transport.requests], ["POST", "GET"])
        client, transport = self.client([page, self.userinfo(True)], self.OAUTH)
        found = client.search_mods(GAME, "ab")
        self.assertEqual([item["mod_id"] for item in found["items"]], [1, 2, 3])
        self.assertEqual((found["total"], transport.requests[1][1]), (3, "https://users.nexusmods.com/oauth/userinfo"))
        # The preference is read again on the next search, never remembered.
        client, transport = self.client([page, self.userinfo(True), page, self.userinfo(False)], self.OAUTH)
        self.assertEqual(len(client.search_mods(GAME, "ab")["items"]), 3)
        self.assertEqual(len(client.search_mods(GAME, "ab")["items"]), 1)

    def test_a_page_without_a_showable_adult_mod_does_not_read_the_preference(self):
        # Adult and hidden, adult with an unknown flag, or only non-adult mods: nothing for the preference to change.
        for nodes in ([node(1)], [node(1, adultContent=True, status="hidden")], [node(1, adultContent=None)], []):
            with self.subTest(nodes=len(nodes)):
                client, transport = self.client([reply(nodes)], self.OAUTH)
                client.search_mods(GAME, "ab")
                self.assertEqual([request[0] for request in transport.requests], ["POST"])

    def test_hidden_mods_count_in_total_but_are_not_listed(self):
        client, _ = self.client([reply([node(1), node(2, adultContent=True), node(3, status="hidden")], total=3)])
        result = client.search_mods(GAME, "ab")
        self.assertEqual(([item["mod_id"] for item in result["items"]], result["total"], result["pages"]), ([1], 3, 1))

    def test_a_failed_preference_read_fails_the_search_instead_of_guessing(self):
        client, _ = self.client([reply([node(1, adultContent=True)]), json_response({"error": "SecretMarker"}, 500)], self.OAUTH)
        with self.assertRaises(nexus.NexusError) as caught:
            client.search_mods(GAME, "ab")
        self.assertEqual(caught.exception.code, "provider")
        client, _ = self.client([reply([node(1, adultContent=True)]), {"sub": "x"}], self.OAUTH)
        with self.assertRaises(nexus.NexusError) as caught:
            client.search_mods(GAME, "ab")
        self.assertEqual(caught.exception.code, "payload")

    def test_provider_failures_map_to_existing_codes_without_provider_text(self):
        secret = {"errors": [{"message": "do-not-print SecretMarker"}]}
        limits = {"Retry-After": "60", "X-RL-Hourly-Remaining": "0"}
        cases = (
            (json_response(secret, 429, limits), "rate_limit"), (json_response(secret, 500), "provider"),
            (json_response(secret, 403), "provider"), (json_response(secret, 401), "provider"),
            (json_response({}, 302, {"Location": "https://evil.test/"}), "redirect"),
            (json_response(secret, 200), "payload"),                          # GraphQL errors are never partial data
            (json_response(dict(reply([node()]), errors=[{"message": "x"}])), "payload"),
            (nexus.Response(200, {}, b"not json SecretMarker"), "payload"), (nexus.Response(200, {}, b""), "payload"),
            (json_response({"data": None}), "payload"), (json_response(reply([node(name=None)])), "payload"),
            (json_response(reply([node(1)], total=5)), "payload"),            # a truncated page
            (json_response(reply([node(i) for i in range(1, 22)])), "payload"),
            (json_response(reply([node(game={"id": 1, "domainName": "othergame"})])), "payload"),
            (nexus.Response(200, {}, b" " * (1048576 + 1)), "size_limit"), (nexus.Response(200, {}, "text"), "payload"))
        for response, code in cases:
            with self.subTest(code=code, status=response.status):
                client, _ = self.client([response])
                with self.assertRaises(nexus.NexusError) as caught:
                    client.search_mods(GAME, "ab")
                self.assertEqual(caught.exception.code, code)
                self.assertNotIn("SecretMarker", repr(caught.exception) + str(caught.exception))
                self.assertNotIn("do-not-print", repr(caught.exception) + str(caught.exception))
        limited, _ = self.client([json_response(secret, 429, limits)])
        with self.assertRaises(nexus.NexusError) as caught:
            limited.search_mods(GAME, "ab")
        self.assertEqual((caught.exception.status, caught.exception.retry_after), (429, 60))
        self.assertEqual(limited.rate_limits["hourly_remaining"], 0)

    def test_transport_failures_propagate_with_their_own_codes(self):
        for code in ("network", "timeout", "size_limit"):
            with self.subTest(code=code):
                transport = mock.Mock()
                transport.request.side_effect = nexus.NexusError(code)
                client = nexus.NexusClient(nexus.Credentials(api_key="fixture-key"), transport)
                with self.assertRaises(nexus.NexusError) as caught:
                    client.search_mods(GAME, "ab")
                self.assertEqual(caught.exception.code, code)

    def test_an_expired_token_asks_to_reconnect_before_any_request(self):
        now = [1000.0]
        transport = FixtureTransport([])
        client = nexus.NexusClient(nexus.Credentials(access_token="expired", refresh_token="refresh", expires_at=999.0),
                                   transport, clock=lambda: now[0])
        with self.assertRaises(nexus.NexusError) as caught:
            client.search_mods(GAME, "ab")
        self.assertEqual(caught.exception.code, "reconnect_required")
        self.assertEqual(transport.requests, [])

    def test_account_backed_client_refreshes_once_on_401_and_resends_the_same_body(self):
        class Account:
            def __init__(self):
                self.forced = []

            def credentials(self, force=False):
                self.forced.append(force)
                return nexus.Credentials(access_token="fresh-token" if force else "stale-token")
        account = Account()
        transport = FixtureTransport([json_response({}, 401), json_response(reply([node()]))])
        client = nexus.NexusClient(account=account, transport=transport)
        result = client.search_mods(GAME, "ab")
        self.assertEqual([item["mod_id"] for item in result["items"]], [17])
        self.assertEqual([request[0] for request in transport.requests], ["POST", "POST"])
        first, second = transport.requests
        self.assertEqual(first[2]["body"], second[2]["body"])
        self.assertEqual((first[2]["headers"]["Authorization"], second[2]["headers"]["Authorization"]),
                         ("Bearer stale-token", "Bearer fresh-token"))
        self.assertEqual(account.forced, [False, True])
        # A second 401 is final.
        account = Account()
        transport = FixtureTransport([json_response({}, 401), json_response({}, 401)])
        client = nexus.NexusClient(account=account, transport=transport)
        with self.assertRaises(nexus.NexusError) as caught:
            client.search_mods(GAME, "ab")
        self.assertEqual((caught.exception.code, caught.exception.status), ("provider", 401))
        self.assertEqual(len(transport.requests), 2)


class SearchTransportTests(SearchTest):
    def test_the_search_body_goes_through_the_graphql_post_allow_list(self):
        class WireResponse:
            status = 200

            def __init__(self):
                self.chunks = [json.dumps(reply([])).encode()]

            def close(self):
                pass

            def getheaders(self):
                return []

            def read1(self, amount):
                return self.chunks.pop(0) if self.chunks else b""
        wire = mock.Mock()
        wire.getresponse.return_value = WireResponse()
        body = search.search_body(GAME, "ab", 1)
        with mock.patch("http.client.HTTPSConnection", return_value=wire) as connection:
            client = nexus.NexusClient(nexus.Credentials(api_key="fixture-key"), nexus.HTTPSTransport())
            self.assertEqual(client.search_mods(GAME, "ab"), {"items": [], "total": 0, "page": 1, "pages": 0})
        self.assertEqual(connection.call_args.args[0], "api.nexusmods.com")
        self.assertEqual(wire.request.call_args.args, ("POST", "/v2/graphql"))
        self.assertEqual(wire.request.call_args.kwargs["body"], body)
        self.assertEqual(wire.request.call_args.kwargs["headers"]["apikey"], "fixture-key")
        self.assertEqual(nexus.transport.POST_URLS, frozenset({"https://api.nexusmods.com/v2/graphql",
                                                               "https://users.nexusmods.com/oauth/token"}))


# ---- Browsing all of a game's mods in a chosen order (no name filter) ----
# Typed out here, so a silent edit of a query constant, a sort field or a direction fails (as HAND_QUERY does for search).
BROWSE_HEAD = ("query ModBridgeBrowse($gameDomainName: String!, $count: Int!, $offset: Int!) { "
               "mods(viewUploaderHidden: false, viewUserBlockedContent: false, "
               "filter: {op: AND, gameDomainName: [{value: $gameDomainName, op: EQUALS}], "
               "status: [{value: \"published\", op: EQUALS}]}, sort: [")
BROWSE_TAIL = ("], offset: $offset, count: $count) { "
               "nodes { modId name summary version author adultContent status uploader { name } game { id domainName } } "
               "nodesCount totalCount } }")
HAND_BROWSE = {"downloads": BROWSE_HEAD + "{downloads: {direction: DESC}}, {name: {direction: ASC}}" + BROWSE_TAIL,
               "endorsements": BROWSE_HEAD + "{endorsements: {direction: DESC}}, {name: {direction: ASC}}" + BROWSE_TAIL,
               "updated": BROWSE_HEAD + "{updatedAt: {direction: DESC}}, {name: {direction: ASC}}" + BROWSE_TAIL,
               "name": BROWSE_HEAD + "{name: {direction: ASC}}" + BROWSE_TAIL}


class BrowseRequestTests(SearchTest):
    def test_each_sort_is_one_fixed_constant_query_with_three_typed_variables(self):
        self.assertEqual(search.BROWSE_QUERIES, HAND_BROWSE)
        self.assertEqual(list(search.BROWSE_SORTS), ["downloads", "endorsements", "updated", "name"])
        self.assertEqual(search.DEFAULT_SORT, "downloads")
        for key, query in search.BROWSE_QUERIES.items():
            with self.subTest(sort=key):
                self.assertEqual(re.findall(r"\$(\w+): (\w+!)", query.split("{", 1)[0]),
                                 [("gameDomainName", "String!"), ("count", "Int!"), ("offset", "Int!")])
                self.assertEqual(sorted(set(re.findall(r"\$(\w+)", query))), ["count", "gameDomainName", "offset"])
                for use in ("{value: $gameDomainName, op: EQUALS}", "offset: $offset, count: $count"):
                    self.assertEqual(query.count(use), 1)
                # No name filter and no relevance sort: this is a listing, not a search.
                self.assertNotIn("name: [", query)
                self.assertNotIn("$name", query)
                self.assertNotIn("relevance", query)
                self.assertNotIn("WILDCARD", query)
                self.assertTrue(query.startswith("query "))
                self.assertNotIn("mutation", query)
                self.assertEqual(query.count("query "), 1)
                # The same listing guards as search: the viewer's own hidden and blocked mods stay out, published only.
                self.assertIn("viewUploaderHidden: false, viewUserBlockedContent: false", query)
                self.assertIn('status: [{value: "published", op: EQUALS}]', query)

    def test_the_sort_mapping_uses_only_documented_sort_fields_and_directions(self):
        # ModsSort fields and SortDirection values read from https://graphql.nexusmods.com/ (see search.py).
        documented_fields = {"relevance", "name", "downloads", "uniqueDownloads", "endorsements", "random", "createdAt",
                             "updatedAt", "size", "lastComment"}
        self.assertEqual(search.BROWSE_SORTS, {"downloads": ("downloads", "DESC"), "endorsements": ("endorsements", "DESC"),
                                              "updated": ("updatedAt", "DESC"), "name": ("name", "ASC")})
        for key, (field, direction) in search.BROWSE_SORTS.items():
            with self.subTest(sort=key):
                self.assertIn(field, documented_fields)
                self.assertIn(direction, ("ASC", "DESC"))
                self.assertIn("{%s: {direction: %s}}" % (field, direction), search.BROWSE_QUERIES[key])
                # Only the sort list differs between the four queries.
                head, rest = search.BROWSE_QUERIES[key].split("sort: [", 1)
                self.assertEqual(head, BROWSE_HEAD.split("sort: [")[0])
                self.assertEqual(rest.split("], offset", 1)[1], BROWSE_TAIL.split("], offset", 1)[1])

    def test_body_bytes_are_the_sorts_constant_query_plus_typed_variables(self):
        for key in search.BROWSE_SORTS:
            with self.subTest(sort=key):
                body = search.browse_body("skyrim", key, 2)
                self.assertIsInstance(body, bytes)
                expected = ('{"query":' + json.dumps(HAND_BROWSE[key]) + ',"variables":{"gameDomainName":"skyrim",'
                            '"count":20,"offset":20}}').encode("ascii")
                self.assertEqual(body, expected)
                record = json.loads(body)
                self.assertEqual(set(record), {"query", "variables"})
                self.assertEqual(list(record["variables"]), ["gameDomainName", "count", "offset"])
                self.assertEqual([type(value) for value in record["variables"].values()], [str, int, int])
                self.assertLess(len(body), search.REQUEST_LIMIT)

    def test_offset_follows_the_page_and_count_is_fixed(self):
        for page, offset in ((1, 0), (2, 20), (3, 40), (50, 980)):
            with self.subTest(page=page):
                variables = json.loads(search.browse_body(GAME, "downloads", page))["variables"]
                self.assertEqual((variables["count"], variables["offset"]), (20, offset))

    def test_only_the_four_sort_keys_are_accepted_and_nothing_is_built_for_others(self):
        for key in search.BROWSE_SORTS:
            self.assertEqual(search.browse_sort(key), key)
        for bad in ("", "Downloads", "downloads ", " name", "relevance", "createdAt", "updatedAt", "uniqueDownloads", "random", "size",
                    "lastComment", "downloads,name", 'name"}', "name: {direction: ASC}", "$offset", None, 1, True, ["downloads"],
                    ("downloads",), {"downloads": 1}, b"downloads"):
            with self.subTest(bad=repr(bad)[:30]):
                with self.assertRaises(nexus.NexusError) as caught:
                    search.browse_sort(bad)
                self.assertEqual(caught.exception.code, "invalid_input")
                with self.assertRaises(nexus.NexusError):
                    search.browse_body(GAME, bad, 1)
        for game, page in (("../../users", 1), ("NAME", 1), ("", 1), (None, 1), (GAME, 0), (GAME, 51), (GAME, True), (GAME, "1"), (GAME, None)):
            with self.subTest(game=game, page=page):
                with self.assertRaises(nexus.NexusError) as caught:
                    search.browse_body(game, "downloads", page)
                self.assertEqual(caught.exception.code, "invalid_input")


class BrowseClientTests(SearchTest):
    def client(self, responses, credentials=None):
        transport = FixtureTransport([item if isinstance(item, nexus.Response) else json_response(item) for item in responses])
        return nexus.NexusClient(credentials or nexus.Credentials(api_key="fixture-key"), transport), transport

    def test_browse_posts_the_sorts_fixed_query_with_the_same_credentials_as_search(self):
        for key in search.BROWSE_SORTS:
            with self.subTest(sort=key):
                client, transport = self.client([reply([node(17), node(18, name="Second")], total=2)])
                result = client.browse_mods(GAME, key)
                self.assertEqual(len(transport.requests), 1)
                method, url, kwargs = transport.requests[0]
                self.assertEqual((method, url), ("POST", "https://api.nexusmods.com/v2/graphql"))
                self.assertEqual(kwargs["body"], search.browse_body(GAME, key, 1))
                self.assertEqual(json.loads(kwargs["body"])["query"], HAND_BROWSE[key])
                self.assertEqual(json.loads(kwargs["body"])["variables"], {"gameDomainName": GAME, "count": 20, "offset": 0})
                self.assertEqual(kwargs["headers"], {"Accept": "application/json", "Application-Name": "PS5ModBridge",
                    "Application-Version": "0.1.0", "apikey": "fixture-key", "Content-Type": "application/json"})
                self.assertEqual(set(kwargs), {"headers", "body"})
                self.assertEqual(set(result), {"items", "total", "page", "pages", "sort"})
                self.assertEqual((result["total"], result["page"], result["pages"], result["sort"]), (2, 1, 1, key))
                self.assertEqual([item["mod_id"] for item in result["items"]], [17, 18])

    def test_the_sort_and_page_default_and_follow_the_arguments(self):
        client, transport = self.client([reply([node(i) for i in range(1, 21)], total=45)])
        result = client.browse_mods(GAME)
        self.assertEqual((result["sort"], result["page"], result["pages"]), ("downloads", 1, 3))
        self.assertEqual(transport.requests[0][2]["body"], search.browse_body(GAME, "downloads", 1))
        client, transport = self.client([reply([node(i) for i in range(1, 6)], total=45)])
        self.assertEqual(client.browse_mods(GAME, "updated", 3)["page"], 3)
        self.assertEqual(json.loads(transport.requests[0][2]["body"])["variables"]["offset"], 40)
        # A deeper page than 50 cannot be asked for, and 5000 mods still make 50 pages.
        client, transport = self.client([reply([node(i) for i in range(1, 21)], total=5000)])
        result = client.browse_mods(GAME, "name", 50)
        self.assertEqual((result["pages"], result["page"]), (50, 50))
        self.assertEqual(json.loads(transport.requests[0][2]["body"])["variables"]["offset"], 980)
        client, _ = self.client([reply([], total=0)])
        self.assertEqual(client.browse_mods(GAME, "name"), {"items": [], "total": 0, "page": 1, "pages": 0, "sort": "name"})

    def test_invalid_input_is_refused_before_any_request(self):
        client, transport = self.client([])
        cases = [("../../users", "downloads", 1), ("NAME", "downloads", 1), ("", "downloads", 1), (None, "downloads", 1),
                 (GAME, "relevance", 1), (GAME, "", 1), (GAME, None, 1), (GAME, "Name", 1), (GAME, "downloads", 0), (GAME, "downloads", 51),
                 (GAME, "downloads", True), (GAME, "downloads", "1"), (GAME, "downloads", None), (GAME, "downloads", 1.0)]
        for game, sort, page in cases:
            with self.subTest(game=game, sort=sort, page=page):
                with self.assertRaises(nexus.NexusError) as caught:
                    client.browse_mods(game, sort, page)
                self.assertEqual(caught.exception.code, "invalid_input")
        self.assertEqual(transport.requests, [])

    def test_a_browse_hit_is_the_same_record_a_search_hit_and_a_latest_mod_is(self):
        rest, _ = self.client([[rest_mod(17), rest_mod(18, name="Second", author="Other", summary="", version="")]])
        latest = rest.latest_mods(GAME)
        client, _ = self.client([reply([node(17), node(18, name="Second", author="Other", summary="", version="")])])
        self.assertEqual(client.browse_mods(GAME, "endorsements")["items"], latest)
        # search_mods keeps its four keys: the sort is a browse-only echo.
        client, _ = self.client([reply([node(17)])])
        self.assertEqual(set(client.search_mods(GAME, "ab")), {"items", "total", "page", "pages"})

    def test_content_rules_are_exactly_those_of_search_for_every_combination(self):
        combos = [(False, "published"), (True, "published"), (None, "published"), (False, "hidden"), (True, "hidden"),
                  (False, "under_moderation"), (False, "Published"), (False, "")]
        page = [node(100 + i, adultContent=adult, status=status) for i, (adult, status) in enumerate(combos)]
        oauth = nexus.Credentials(access_token="opaque-fixture", refresh_token="refresh")
        for label, credentials, info in (("api key", None, None), ("oauth adult", oauth, True), ("oauth not adult", oauth, False)):
            with self.subTest(label=label):
                answers = [reply(page)] + ([{"sub": "42", "name": "OAuth fixture", "membership_roles": ["member"],
                                             "preferences": {"adult": info}}] if info is not None else [])
                searched, search_transport = self.client(list(answers), credentials)
                browsed, browse_transport = self.client(list(answers), credentials)
                found, listed = searched.search_mods(GAME, "ab"), browsed.browse_mods(GAME, "downloads")
                self.assertEqual(listed["items"], found["items"])
                self.assertEqual((listed["total"], listed["pages"]), (found["total"], found["pages"]))
                # The same lookups: one listing, then at most one account-preference read, both times.
                self.assertEqual([request[1] for request in browse_transport.requests], [request[1] for request in search_transport.requests])
                shown = [item["mod_id"] for item in listed["items"]]
                self.assertEqual(shown, [100, 101] if label == "oauth adult" else [100])
                self.assertEqual(listed["total"], len(combos), "total counts hidden mods too")

    def test_a_reply_that_is_not_exactly_the_page_asked_for_is_a_payload_error(self):
        bad = [reply([node(1)], total=45),                                  # a page 1 of 45 must hold 20
               reply([node(i) for i in range(1, 22)], total=45),            # more than 20 nodes
               reply([node(1, game={"id": 444, "domainName": "othergame"})]),  # a hit for another game
               reply([node(1), node(1)]), reply([node(1)], nodesCount=2), {"errors": [{"message": "x"}], "data": None},
               {"data": {"mods": None}}, b"not json", b"[]"]
        for item in bad:
            with self.subTest(item=repr(item)[:60]):
                client, _ = self.client([nexus.Response(200, {}, item) if isinstance(item, bytes) else item])
                with self.assertRaises(nexus.NexusError) as caught:
                    client.browse_mods(GAME, "downloads")
                self.assertEqual(caught.exception.code, "payload")
        # A page past the end is a normal answer; a shifted one is not.
        client, _ = self.client([reply([], total=25)])
        self.assertEqual(client.browse_mods(GAME, "name", 3), {"items": [], "total": 25, "page": 3, "pages": 2, "sort": "name"})
        client, _ = self.client([reply([node(1)], total=25)])
        with self.assertRaises(nexus.NexusError):
            client.browse_mods(GAME, "name", 3)

    def test_provider_status_errors_keep_their_codes(self):
        for status, code in ((429, "rate_limit"), (500, "provider"), (302, "redirect")):
            with self.subTest(status=status):
                client, _ = self.client([nexus.Response(status, {}, b"{}")])
                with self.assertRaises(nexus.NexusError) as caught:
                    client.browse_mods(GAME, "downloads")
                self.assertEqual((caught.exception.code, caught.exception.status), (code, status))


if __name__ == "__main__":
    unittest.main()
