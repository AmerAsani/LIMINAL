// V13.2/V13.3: Tests und Bots nutzen die Transportschicht statisch (dieselbe wie liminal_net.dll, ohne DLL-Laden)
#include "coop/netapi.hpp"

namespace lim::coop {

void staticNetApi(NetApi& a) {
    a.host = ln_host;
    a.join = ln_join;
    a.poll = ln_poll;
    a.send = ln_send;
    a.drop = ln_drop;
    a.stats = ln_stats;
    a.code = ln_code;
    a.port = ln_port;
    a.close = ln_close;
    a.random = ln_random;
    a.version = ln_version;
}

}  // namespace lim::coop
