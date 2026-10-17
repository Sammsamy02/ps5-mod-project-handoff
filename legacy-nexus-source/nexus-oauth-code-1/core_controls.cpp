#include "modbridge/nexus.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
namespace {
unsigned checks=0,failed=0;
void expect(bool ok,const char *name) { ++checks; if(!ok) { ++failed; std::cerr<<name<<" failed\n"; } }
template<class T> bool zero(const T& value) { return std::all_of(value.begin(),value.end(),[](auto c){return c==0;}); }
constexpr auto verifier="dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
const std::string valid="{\"access_token\":\"fake-access\",\"refresh_token\":\"fake-refresh\",\"token_type\":\"bEaReR\",\"expires_in\":3600,\"scope\":\"openid profile\"}";
bool decode(std::string_view text,std::array<char,16385>& access,std::array<char,16385>& refresh,std::size_t& a,std::size_t& r,std::uint32_t& expiry) {
 return modbridge::nexus_decode_oauth_tokens({reinterpret_cast<const std::uint8_t*>(text.data()),text.size()},access,a,refresh,r,expiry);
}
void rejected(std::string text,const char *name) {
 std::array<char,16385> access,refresh; access.fill('x');refresh.fill('x');
 std::size_t a=99,r=99;std::uint32_t expiry=99;
 expect(!decode(text,access,refresh,a,r,expiry)&&!a&&!r&&!expiry&&zero(access)&&zero(refresh),name);
}
}
int main() {
 std::array<char,65536> form; form.fill('x');std::size_t written=99;
 const std::string want="grant_type=authorization_code&redirect_uri=http%3A%2F%2F127.0.0.1%3A8085%2Fauth%2Fcallback&client_id=fake_app&code=fake%2Bcode%26%3D%3F&code_verifier="+std::string(verifier);
 expect(modbridge::nexus_oauth_code_form("fake_app","http://127.0.0.1:8085/auth/callback","fake+code&=?",verifier,form,written)&&written==want.size()&&std::string_view(form.data(),written)==want&&form[written]==0&&std::all_of(form.begin()+written,form.end(),[](char c){return !c;}),"canonical_form");
 std::array<char,20> short_form;short_form.fill('x');written=99;
 expect(!modbridge::nexus_oauth_code_form("fake_app","http://127.0.0.1:8085/auth/callback","fake",verifier,short_form,written)&&!written&&zero(short_form),"short_form_whole_wipe");
 for(const auto& client:std::vector<std::string>{"public_test","bad app","","bad+id",std::string(129,'a')}) {
  form.fill('x');written=99;
  expect(!modbridge::nexus_oauth_code_form(client,"http://127.0.0.1:8085/auth/callback","fake",verifier,form,written)&&!written&&zero(form),"invalid_client");
 }
 for(const auto& redirect:std::vector<std::string>{"http://user@127.0.0.1/auth/callback","http://127.0.0.1/auth/callback?x=1","http://127.0.0.1/auth/callback#x","http://127.0.0.1/other","http://127.0.0.1\\evil/auth/callback","ftp://127.0.0.1/auth/callback","http://127.0.0.1:65536/auth/callback","http://127.0.0.1:/auth/callback"}) {
  form.fill('x');written=99;
  expect(!modbridge::nexus_oauth_code_form("fake_app",redirect,"fake",verifier,form,written)&&!written&&zero(form),"invalid_redirect");
 }
 form.fill('x');written=99;
 expect(modbridge::nexus_oauth_code_form("fake_app","http://ps5-host-not-configured.invalid:8085/auth/callback",std::string(2048,'+'),verifier,form,written),"native_private_ip_callback_and_code_max");
 for(const auto& code:std::vector<std::string>{"",std::string(2049,'a'),"bad code",std::string("bad\0code",8),std::string(1,char(127))}) {
  form.fill('x');written=99;
  expect(!modbridge::nexus_oauth_code_form("fake_app","http://127.0.0.1/auth/callback",code,verifier,form,written)&&!written&&zero(form),"invalid_code");
 }
 for(const auto& v:std::vector<std::string>{std::string(42,'a'),std::string(129,'a'),std::string(43,'+')}) {
  form.fill('x');written=99;
  expect(!modbridge::nexus_oauth_code_form("fake_app","http://127.0.0.1/auth/callback","fake",v,form,written)&&!written&&zero(form),"invalid_verifier");
 }
 std::strcpy(form.data(),"fake_app");written=99;const auto aliased=form;
 expect(!modbridge::nexus_oauth_code_form({form.data(),8},"http://127.0.0.1/auth/callback","fake",verifier,form,written)&&written==99&&form==aliased,"form_alias_untouched");
 std::array<char,16385> access,refresh;access.fill('x');refresh.fill('x');std::size_t a=99,r=99;std::uint32_t expiry=99;
 expect(decode(valid,access,refresh,a,r,expiry)&&a==11&&r==12&&expiry==3600&&std::string_view(access.data(),a)=="fake-access"&&std::string_view(refresh.data(),r)=="fake-refresh"&&access[a]==0&&refresh[r]==0,"whole_token_pair");
 rejected("{}","missing_tokens");
 rejected("{\"access_token\":\"fake\",\"refresh_token\":\"fake\",\"token_type\":\"Bearer\",\"expires_in\":1,\"expires_in\":2}","duplicate_token_field");
 rejected(valid+" trailing","trailing_input");
 for(const auto& seconds:std::vector<std::string>{"0","-1","31536001","true","\"3600\"","3600.0","3.6e3"})
  rejected("{\"access_token\":\"fake\",\"refresh_token\":\"fake\",\"token_type\":\"Bearer\",\"expires_in\":"+seconds+"}","integer_lifetime_only");
 for(const auto& secret:std::vector<std::string>{"","bad token","bad\\u0000token","bad\\u00E9token",std::string(16385,'a')})
  rejected("{\"access_token\":\""+secret+"\",\"refresh_token\":\"fake\",\"token_type\":\"Bearer\",\"expires_in\":1}","invalid_access_token");
 rejected("{\"access_token\":\"fake\",\"refresh_token\":null,\"token_type\":\"Bearer\",\"expires_in\":1}","required_refresh_token");
 rejected("{\"access_token\":\"fake\",\"refresh_token\":\"fake\",\"token_type\":\"Basic\",\"expires_in\":1}","required_bearer_type");
 std::string escaped;
 for(unsigned i=0;i<16384;++i)escaped+="\\\"";
 const std::string maximum="{\"access_token\":\""+escaped+"\",\"refresh_token\":\""+escaped+"\",\"token_type\":\"Bearer\",\"expires_in\":31536000}";
 expect(decode(maximum,access,refresh,a,r,expiry)&&a==16384&&r==16384&&expiry==31536000&&std::all_of(access.begin(),access.begin()+a,[](char c){return c=='\"';})&&std::all_of(refresh.begin(),refresh.begin()+r,[](char c){return c=='\"';}),"fully_escaped_maximum_pair");
 rejected(valid+std::string(131073-valid.size(),' '),"raw_response_cap");
 rejected("{\"unknown\":"+std::string(64,'[')+"0"+std::string(64,']')+"}","parser_depth_gate");
 access.fill('x');refresh.fill('x');a=99;r=99;expiry=99;
 expect(!modbridge::nexus_decode_oauth_tokens({reinterpret_cast<const std::uint8_t*>(valid.data()),valid.size()},{access.data(),5},a,refresh,r,expiry)&&!a&&!r&&!expiry&&std::all_of(access.begin(),access.begin()+5,[](char c){return !c;})&&zero(refresh),"short_token_output_whole_wipe");
 access.fill('x');a=99;r=99;expiry=99;const auto before=access;
 expect(!modbridge::nexus_decode_oauth_tokens({reinterpret_cast<const std::uint8_t*>(valid.data()),valid.size()},access,a,access,r,expiry)&&access==before&&a==99&&r==99&&expiry==99,"token_alias_untouched");
 std::cout<<"{\"checks\":"<<checks<<",\"failed\":"<<failed<<"}\n";
 return failed?1:0;
}
