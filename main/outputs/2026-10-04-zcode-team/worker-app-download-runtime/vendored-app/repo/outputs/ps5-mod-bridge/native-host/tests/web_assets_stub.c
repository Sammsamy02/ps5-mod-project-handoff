#include "http.h"

static const unsigned char test_index_html[] =
    "<!doctype html><link rel=\"stylesheet\" href=\"/style.css\">"
    "<main id=\"app\"></main><script src=\"/app.js\"></script>";
static const unsigned char test_app_js[] = "/* diagnostics-only test stub */";
static const unsigned char test_style_css[] = "/* diagnostics-only test stub */";

const struct http_static_asset http_asset_index_html = {
    test_index_html, sizeof(test_index_html) - 1
};
const struct http_static_asset http_asset_app_js = {
    test_app_js, sizeof(test_app_js) - 1
};
const struct http_static_asset http_asset_style_css = {
    test_style_css, sizeof(test_style_css) - 1
};
