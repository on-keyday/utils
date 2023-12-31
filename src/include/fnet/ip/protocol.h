/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <core/byte.h>

namespace utils::fnet::ip {
    enum class Version {
        unspec,
        ipv4,
        ipv6,
    };

    enum class Protocol : byte {
        hopopt = 0,
        icmp = 1,
        igmp = 2,
        ggp = 3,
        ipv4 = 4,
        st = 5,
        tcp = 6,
        cbt = 7,
        egp = 8,
        igp = 9,
        bbn_rcc_mon = 10,
        nvp_ii = 11,
        pup = 12,
        argus = 13,
        emcon = 14,
        xnet = 15,
        chaos = 16,
        udp = 17,
        mux = 18,
        dcn_meas = 19,
        hmp = 20,
        prm = 21,
        xns_idp = 22,
        trunk_1 = 23,
        trunk_2 = 24,
        leaf_1 = 25,
        leaf_2 = 26,
        rdp = 27,
        irtp = 28,
        iso_tp4 = 29,
        netblt = 30,
        mfe_nsp = 31,
        merit_inp = 32,
        dccp = 33,
        three_pc = 34,
        idpr = 35,
        xtp = 36,
        ddp = 37,
        idpr_cmtp = 38,
        tp_plus_plus = 39,
        il = 40,
        ipv6 = 41,
        sdrp = 42,
        ipv6_route = 43,
        ipv6_frag = 44,
        idrp = 45,
        rsvp = 46,
        gre = 47,
        dsr = 48,
        bna = 49,
        esp = 50,
        ah = 51,
        i_nlsp = 52,
        swipe = 53,
        narp = 54,
        mobile = 55,
        tlsp = 56,
        skip = 57,
        ipv6_icmp = 58,
        ipv6_nonxt = 59,
        ipv6_opts = 60,
        any_host_internal_protocol = 61,
        cftp = 62,
        any_local_network = 63,
        sat_expak = 64,
        kryptolan = 65,
        rvd = 66,
        ippc = 67,
        any_distributed_file_system = 68,
        sat_mon = 69,
        visa = 70,
        ipcv = 71,
        cpnx = 72,
        cphb = 73,
        wsn = 74,
        pvp = 75,
        br_sat_mon = 76,
        sun_nd = 77,
        wb_mon = 78,
        wb_expak = 79,
        iso_ip = 80,
        vmtp = 81,
        secure_vmtp = 82,
        vines = 83,
        iptm = 84,
        ttp = iptm,
        nsfnet_igp = 85,
        dgp = 86,
        tcf = 87,
        eigrp = 88,
        ospfigp = 89,
        sprite_rpc = 90,
        larp = 91,
        mtp = 92,
        ax_25 = 93,
        ipip = 94,
        micp = 95,
        scc_sp = 96,
        etherip = 97,
        encap = 98,
        any_private_encryption_scheme = 99,
        gmtp = 100,
        ifmp = 101,
        pnni = 102,
        pim = 103,
        aris = 104,
        scps = 105,
        qnx = 106,
        a_n = 107,
        ipcomp = 108,
        snp = 109,
        compaq_peer = 110,
        ipx_in_ip = 111,
        vrrp = 112,
        pgm = 113,
        any_0_hop_protocol = 114,
        l2tp = 115,
        ddx = 116,
        iatp = 117,
        stp = 118,
        srp = 119,
        uti = 120,
        smp = 121,
        sm = 122,
        ptp = 123,
        isis_over_ipv4 = 124,
        fire = 125,
        crtp = 126,
        crudp = 127,
        sscopmce = 128,
        iplt = 129,
        sps = 130,
        pipe = 131,
        sctp = 132,
        fc = 133,
        rsvp_e2e_ignore = 134,
        mobility_header = 135,
        udplite = 136,
        mpls_in_ip = 137,
        manet = 138,
        hip = 139,
        shim6 = 140,
        wesp = 141,
        rohc = 142,
        ethernet = 143,
        aggfrag = 144,
        nsh = 145,
        unassigned1 = 146,
        unassigned2 = 147,
        unassigned3 = 148,
        unassigned4 = 149,
        unassigned5 = 150,
        unassigned6 = 151,
        unassigned7 = 152,
        unassigned8 = 153,
        unassigned9 = 154,
        unassigned10 = 155,
        unassigned11 = 156,
        unassigned12 = 157,
        unassigned13 = 158,
        unassigned14 = 159,
        unassigned15 = 160,
        unassigned16 = 161,
        unassigned17 = 162,
        unassigned18 = 163,
        unassigned19 = 164,
        unassigned20 = 165,
        unassigned21 = 166,
        unassigned22 = 167,
        unassigned23 = 168,
        unassigned24 = 169,
        unassigned25 = 170,
        unassigned26 = 171,
        unassigned27 = 172,
        unassigned28 = 173,
        unassigned29 = 174,
        unassigned30 = 175,
        unassigned31 = 176,
        unassigned32 = 177,
        unassigned33 = 178,
        unassigned34 = 179,
        unassigned35 = 180,
        unassigned36 = 181,
        unassigned37 = 182,
        unassigned38 = 183,
        unassigned39 = 184,
        unassigned40 = 185,
        unassigned41 = 186,
        unassigned42 = 187,
        unassigned43 = 188,
        unassigned44 = 189,
        unassigned45 = 190,
        unassigned46 = 191,
        unassigned47 = 192,
        unassigned48 = 193,
        unassigned49 = 194,
        unassigned50 = 195,
        unassigned51 = 196,
        unassigned52 = 197,
        unassigned53 = 198,
        unassigned54 = 199,
        unassigned55 = 200,
        unassigned56 = 201,
        unassigned57 = 202,
        unassigned58 = 203,
        unassigned59 = 204,
        unassigned60 = 205,
        unassigned61 = 206,
        unassigned62 = 207,
        unassigned63 = 208,
        unassigned64 = 209,
        unassigned65 = 210,
        unassigned66 = 211,
        unassigned67 = 212,
        unassigned68 = 213,
        unassigned69 = 214,
        unassigned70 = 215,
        unassigned71 = 216,
        unassigned72 = 217,
        unassigned73 = 218,
        unassigned74 = 219,
        unassigned75 = 220,
        unassigned76 = 221,
        unassigned77 = 222,
        unassigned78 = 223,
        unassigned79 = 224,
        unassigned80 = 225,
        unassigned81 = 226,
        unassigned82 = 227,
        unassigned83 = 228,
        unassigned84 = 229,
        unassigned85 = 230,
        unassigned86 = 231,
        unassigned87 = 232,
        unassigned88 = 233,
        unassigned89 = 234,
        unassigned90 = 235,
        unassigned91 = 236,
        unassigned92 = 237,
        unassigned93 = 238,
        unassigned94 = 239,
        unassigned95 = 240,
        unassigned96 = 241,
        unassigned97 = 242,
        unassigned98 = 243,
        unassigned99 = 244,
        unassigned100 = 245,
        unassigned101 = 246,
        unassigned102 = 247,
        unassigned103 = 248,
        unassigned104 = 249,
        unassigned105 = 250,
        unassigned106 = 251,
        unassigned107 = 252,
        use_for_experimentation_and_testing1 = 253,
        use_for_experimentation_and_testing2 = 254,
        reserved = 255,
    };

    constexpr const char* description(Protocol protocol) {
        switch (protocol) {
            case Protocol::hopopt:
                return "IPv6 Hop-by-Hop Option";
            case Protocol::icmp:
                return "Internet Control Message";
            case Protocol::igmp:
                return "Internet Group Management";
            case Protocol::ggp:
                return "Gateway-to-Gateway";
            case Protocol::ipv4:
                return "IPv4 encapsulation";
            case Protocol::st:
                return "Stream";
            case Protocol::tcp:
                return "Transmission Control";
            case Protocol::cbt:
                return "CBT";
            case Protocol::egp:
                return "Exterior Gateway Protocol";
            case Protocol::igp:
                return "any private interior gateway (used by Cisco for their IGRP)";
            case Protocol::bbn_rcc_mon:
                return "BBN RCC Monitoring";
            case Protocol::nvp_ii:
                return "Network Voice Protocol";
            case Protocol::pup:
                return "PUP";
            case Protocol::argus:
                return "ARGUS";
            case Protocol::emcon:
                return "EMCON";
            case Protocol::xnet:
                return "Cross Net Debugger";
            case Protocol::chaos:
                return "Chaos";
            case Protocol::udp:
                return "User Datagram";
            case Protocol::mux:
                return "Multiplexing";
            case Protocol::dcn_meas:
                return "DCN Measurement Subsystems";
            case Protocol::hmp:
                return "Host Monitoring";
            case Protocol::prm:
                return "Packet Radio Measurement";
            case Protocol::xns_idp:
                return "XEROX NS IDP";
            case Protocol::trunk_1:
                return "Trunk-1";
            case Protocol::trunk_2:
                return "Trunk-2";
            case Protocol::leaf_1:
                return "Leaf-1";
            case Protocol::leaf_2:
                return "Leaf-2";
            case Protocol::rdp:
                return "Reliable Data Protocol";
            case Protocol::irtp:
                return "Internet Reliable Transaction";
            case Protocol::iso_tp4:
                return "ISO Transport Protocol Class 4";
            case Protocol::netblt:
                return "Bulk Data Transfer Protocol";
            case Protocol::mfe_nsp:
                return "MFE Network Services Protocol";
            case Protocol::merit_inp:
                return "MERIT Internodal Protocol";
            case Protocol::dccp:
                return "Datagram Congestion Control Protocol";
            case Protocol::three_pc:
                return "Third Party Connect Protocol";
            case Protocol::idpr:
                return "Inter-Domain Policy Routing Protocol";
            case Protocol::xtp:
                return "XTP";
            case Protocol::ddp:
                return "Datagram Delivery Protocol";
            case Protocol::idpr_cmtp:
                return "IDPR Control Message Transport Proto";
            case Protocol::tp_plus_plus:
                return "TP++ Transport Protocol";
            case Protocol::il:
                return "IL Transport Protocol";
            case Protocol::ipv6:
                return "IPv6 encapsulation";
            case Protocol::sdrp:
                return "Source Demand Routing Protocol";
            case Protocol::ipv6_route:
                return "Routing Header for IPv6";
            case Protocol::ipv6_frag:
                return "Fragment Header for IPv6";
            case Protocol::idrp:
                return "Inter-Domain Routing Protocol";
            case Protocol::rsvp:
                return "Reservation Protocol";
            case Protocol::gre:
                return "Generic Routing Encapsulation";
            case Protocol::dsr:
                return "Dynamic Source Routing Protocol";
            case Protocol::bna:
                return "BNA";
            case Protocol::esp:
                return "Encap Security Payload";
            case Protocol::ah:
                return "Authentication Header";
            case Protocol::i_nlsp:
                return "Integrated Net Layer Security  TUBA";
            case Protocol::swipe:
                return "IP with Encryption";
            case Protocol::narp:
                return "NBMA Address Resolution Protocol";
            case Protocol::mobile:
                return "IP Mobility";
            case Protocol::tlsp:
                return "Transport Layer Security Protocol using Kryptonet key management";
            case Protocol::skip:
                return "SKIP";
            case Protocol::ipv6_icmp:
                return "ICMP for IPv6";
            case Protocol::ipv6_nonxt:
                return "No Next Header for IPv6";
            case Protocol::ipv6_opts:
                return "Destination Options for IPv6";
            case Protocol::any_host_internal_protocol:
                return "any host internal protocol";
            case Protocol::cftp:
                return "CFTP";
            case Protocol::any_local_network:
                return "any local network";
            case Protocol::sat_expak:
                return "SATNET and Backroom EXPAK";
            case Protocol::kryptolan:
                return "Kryptolan";
            case Protocol::rvd:
                return "MIT Remote Virtual Disk Protocol";
            case Protocol::ippc:
                return "Internet Pluribus Packet Core";
            case Protocol::any_distributed_file_system:
                return "any distributed file system";
            case Protocol::sat_mon:
                return "SATNET Monitoring";
            case Protocol::visa:
                return "VISA Protocol";
            case Protocol::ipcv:
                return "Internet Packet Core Utility";
            case Protocol::cpnx:
                return "Computer Protocol Network Executive";
            case Protocol::cphb:
                return "Computer Protocol Heart Beat";
            case Protocol::wsn:
                return "Wang Span Network";
            case Protocol::pvp:
                return "Packet Video Protocol";
            case Protocol::br_sat_mon:
                return "Backroom SATNET Monitoring";
            case Protocol::sun_nd:
                return "SUN ND PROTOCOL-Temporary";
            case Protocol::wb_mon:
                return "WIDEBAND Monitoring";
            case Protocol::wb_expak:
                return "WIDEBAND EXPAK";
            case Protocol::iso_ip:
                return "ISO Internet Protocol";
            case Protocol::vmtp:
                return "VMTP";
            case Protocol::secure_vmtp:
                return "SECURE-VMTP";
            case Protocol::vines:
                return "VINES";
            case Protocol::iptm:
                return "Internet Protocol Traffic Manager";
            case Protocol::nsfnet_igp:
                return "NSFNET-IGP";
            case Protocol::dgp:
                return "Dissimilar Gateway Protocol";
            case Protocol::tcf:
                return "TCF";
            case Protocol::eigrp:
                return "EIGRP";
            case Protocol::ospfigp:
                return "OSPFIGP";
            case Protocol::sprite_rpc:
                return "Sprite RPC Protocol";
            case Protocol::larp:
                return "Locus Address Resolution Protocol";
            case Protocol::mtp:
                return "Multicast Transport Protocol";
            case Protocol::ax_25:
                return "AX.25 Frames";
            case Protocol::ipip:
                return "IP-within-IP Encapsulation Protocol";
            case Protocol::micp:
                return "Mobile Internetworking Control Pro.";
            case Protocol::scc_sp:
                return "Semaphore Communications Sec. Pro.";
            case Protocol::etherip:
                return "Ethernet-within-IP Encapsulation";
            case Protocol::encap:
                return "Encapsulation Header";
            case Protocol::any_private_encryption_scheme:
                return "any private encryption scheme";
            case Protocol::gmtp:
                return "GMTP";
            case Protocol::ifmp:
                return "Ipsilon Flow Management Protocol";
            case Protocol::pnni:
                return "PNNI over IP";
            case Protocol::pim:
                return "Protocol Independent Multicast";
            case Protocol::aris:
                return "ARIS";
            case Protocol::scps:
                return "SCPS";
            case Protocol::qnx:
                return "QNX";
            case Protocol::a_n:
                return "Active Networks";
            case Protocol::ipcomp:
                return "IP Payload Compression Protocol";
            case Protocol::snp:
                return "Sitara Networks Protocol";
            case Protocol::compaq_peer:
                return "Compaq Peer Protocol";
            case Protocol::ipx_in_ip:
                return "IPX in IP";
            case Protocol::vrrp:
                return "Virtual Router Redundancy Protocol";
            case Protocol::pgm:
                return "PGM Reliable Transport Protocol";
            case Protocol::any_0_hop_protocol:
                return "any 0-hop protocol";
            case Protocol::l2tp:
                return "Layer Two Tunneling Protocol";
            case Protocol::ddx:
                return "D-II Data Exchange (DDX)";
            case Protocol::iatp:
                return "Interactive Agent Transfer Protocol";
            case Protocol::stp:
                return "Schedule Transfer Protocol";
            case Protocol::srp:
                return "SpectraLink Radio Protocol";
            case Protocol::uti:
                return "UTI";
            case Protocol::smp:
                return "Simple Message Protocol";
            case Protocol::sm:
                return "Simple Multicast Protocol";
            case Protocol::ptp:
                return "Performance Transparency Protocol";
            case Protocol::isis_over_ipv4:
                return "ISIS over IPv4";
            case Protocol::fire:
                return "FIRE";
            case Protocol::crtp:
                return "Combat Radio Transport Protocol";
            case Protocol::crudp:
                return "Combat Radio User Datagram";
            case Protocol::sscopmce:
                return "SSCOPMCE";
            case Protocol::iplt:
                return "IPLT";
            case Protocol::sps:
                return "Secure Packet Shield";
            case Protocol::pipe:
                return "Private IP Encapsulation within IP";
            case Protocol::sctp:
                return "Stream Control Transmission Protocol";
            case Protocol::fc:
                return "Fibre Channel";
            case Protocol::rsvp_e2e_ignore:
                return "RSVP-E2E-IGNORE";
            case Protocol::mobility_header:
                return "Mobility Header";
            case Protocol::udplite:
                return "UDPLite";
            case Protocol::mpls_in_ip:
                return "MPLS-in-IP";
            case Protocol::manet:
                return "MANET Protocols";
            case Protocol::hip:
                return "Host Identity Protocol";
            case Protocol::shim6:
                return "Shim6 Protocol";
            case Protocol::wesp:
                return "Wrapped Encapsulating Security Payload";
            case Protocol::rohc:
                return "Robust Header Compression";
            case Protocol::ethernet:
                return "Ethernet";
            case Protocol::aggfrag:
                return "AGGFRAG encapsulation payload for ESP";
            case Protocol::nsh:
                return "Network Service Header";
            case Protocol::unassigned1:
            case Protocol::unassigned2:
            case Protocol::unassigned3:
            case Protocol::unassigned4:
            case Protocol::unassigned5:
            case Protocol::unassigned6:
            case Protocol::unassigned7:
            case Protocol::unassigned8:
            case Protocol::unassigned9:
            case Protocol::unassigned10:
            case Protocol::unassigned11:
            case Protocol::unassigned12:
            case Protocol::unassigned13:
            case Protocol::unassigned14:
            case Protocol::unassigned15:
            case Protocol::unassigned16:
            case Protocol::unassigned17:
            case Protocol::unassigned18:
            case Protocol::unassigned19:
            case Protocol::unassigned20:
            case Protocol::unassigned21:
            case Protocol::unassigned22:
            case Protocol::unassigned23:
            case Protocol::unassigned24:
            case Protocol::unassigned25:
            case Protocol::unassigned26:
            case Protocol::unassigned27:
            case Protocol::unassigned28:
            case Protocol::unassigned29:
            case Protocol::unassigned30:
            case Protocol::unassigned31:
            case Protocol::unassigned32:
            case Protocol::unassigned33:
            case Protocol::unassigned34:
            case Protocol::unassigned35:
            case Protocol::unassigned36:
            case Protocol::unassigned37:
            case Protocol::unassigned38:
            case Protocol::unassigned39:
            case Protocol::unassigned40:
            case Protocol::unassigned41:
            case Protocol::unassigned42:
            case Protocol::unassigned43:
            case Protocol::unassigned44:
            case Protocol::unassigned45:
            case Protocol::unassigned46:
            case Protocol::unassigned47:
            case Protocol::unassigned48:
            case Protocol::unassigned49:
            case Protocol::unassigned50:
            case Protocol::unassigned51:
            case Protocol::unassigned52:
            case Protocol::unassigned53:
            case Protocol::unassigned54:
            case Protocol::unassigned55:
            case Protocol::unassigned56:
            case Protocol::unassigned57:
            case Protocol::unassigned58:
            case Protocol::unassigned59:
            case Protocol::unassigned60:
            case Protocol::unassigned61:
            case Protocol::unassigned62:
            case Protocol::unassigned63:
            case Protocol::unassigned64:
            case Protocol::unassigned65:
            case Protocol::unassigned66:
            case Protocol::unassigned67:
            case Protocol::unassigned68:
            case Protocol::unassigned69:
            case Protocol::unassigned70:
            case Protocol::unassigned71:
            case Protocol::unassigned72:
            case Protocol::unassigned73:
            case Protocol::unassigned74:
            case Protocol::unassigned75:
            case Protocol::unassigned76:
            case Protocol::unassigned77:
            case Protocol::unassigned78:
            case Protocol::unassigned79:
            case Protocol::unassigned80:
            case Protocol::unassigned81:
            case Protocol::unassigned82:
            case Protocol::unassigned83:
            case Protocol::unassigned84:
            case Protocol::unassigned85:
            case Protocol::unassigned86:
            case Protocol::unassigned87:
            case Protocol::unassigned88:
            case Protocol::unassigned89:
            case Protocol::unassigned90:
            case Protocol::unassigned91:
            case Protocol::unassigned92:
            case Protocol::unassigned93:
            case Protocol::unassigned94:
            case Protocol::unassigned95:
            case Protocol::unassigned96:
            case Protocol::unassigned97:
            case Protocol::unassigned98:
            case Protocol::unassigned99:
            case Protocol::unassigned100:
            case Protocol::unassigned101:
            case Protocol::unassigned102:
            case Protocol::unassigned103:
            case Protocol::unassigned104:
            case Protocol::unassigned105:
            case Protocol::unassigned106:
            case Protocol::unassigned107:
                return "Unassigned";
            case Protocol::use_for_experimentation_and_testing1:
                return "Use for experimentation and testing";
            case Protocol::use_for_experimentation_and_testing2:
                return "Use for experimentation and testing";
            case Protocol::reserved:
                return "Reserved";
        }
    }

    constexpr const char* to_string(Protocol protocol) {
        switch (protocol) {
            case Protocol::hopopt:
                return "HOP_OPT";
            case Protocol::icmp:
                return "ICMP";
            case Protocol::igmp:
                return "IGMP";
            case Protocol::ggp:
                return "GGP";
            case Protocol::ipv4:
                return "IP_IN_IP";
            case Protocol::st:
                return "ST";
            case Protocol::tcp:
                return "TCP";
            case Protocol::cbt:
                return "CBT";
            case Protocol::egp:
                return "EGP";
            case Protocol::igp:
                return "IGP";
            case Protocol::bbn_rcc_mon:
                return "BBN_RCC_MON";
            case Protocol::nvp_ii:
                return "NVP_II";
            case Protocol::pup:
                return "PUP";
            case Protocol::argus:
                return "ARGUS";
            case Protocol::emcon:
                return "EMCON";
            case Protocol::xnet:
                return "XNET";
            case Protocol::chaos:
                return "CHAOS";
            case Protocol::udp:
                return "UDP";
            case Protocol::mux:
                return "MUX";
            case Protocol::dcn_meas:
                return "DCN_MEAS";
            case Protocol::hmp:
                return "HMP";
            case Protocol::prm:
                return "PRM";
            case Protocol::xns_idp:
                return "XNS_IDP";
            case Protocol::trunk_1:
                return "TRUNK_1";
            case Protocol::trunk_2:
                return "TRUNK_2";
            case Protocol::leaf_1:
                return "LEAF_1";
            case Protocol::leaf_2:
                return "LEAF_2";
            case Protocol::rdp:
                return "RDP";
            case Protocol::irtp:
                return "IRTP";
            case Protocol::iso_tp4:
                return "ISO_TP4";
            case Protocol::netblt:
                return "NETBLT";
            case Protocol::mfe_nsp:
                return "MFE_NSP";
            case Protocol::merit_inp:
                return "MERIT_INP";
            case Protocol::dccp:
                return "DCCP";
            case Protocol::three_pc:
                return "THREE_PC";
            case Protocol::idpr:
                return "IDPR";
            case Protocol::xtp:
                return "XTP";
            case Protocol::ddp:
                return "DDP";
            case Protocol::idpr_cmtp:
                return "IDPR_CMTP";
            case Protocol::tp_plus_plus:
                return "TP_PLUS_PLUS";
            case Protocol::il:
                return "IL";
            case Protocol::ipv6:
                return "IPV6";
            case Protocol::sdrp:
                return "SDRP";
            case Protocol::ipv6_route:
                return "IPV6_ROUTE";
            case Protocol::ipv6_frag:
                return "IPV6_FRAG";
            case Protocol::idrp:
                return "IDRP";
            case Protocol::rsvp:
                return "RSVP";
            case Protocol::gre:
                return "GRE";
            case Protocol::dsr:
                return "DSR";
            case Protocol::bna:
                return "BNA";
            case Protocol::esp:
                return "ESP";
            case Protocol::ah:
                return "AH";
            case Protocol::i_nlsp:
                return "I_NLSP";
            case Protocol::swipe:
                return "SWIPE";
            case Protocol::narp:
                return "NARP";
            case Protocol::mobile:
                return "MOBILE";
            case Protocol::tlsp:
                return "TLSP";
            case Protocol::skip:
                return "SKIP";
            case Protocol::ipv6_icmp:
                return "IPV6_ICMP";
            case Protocol::ipv6_nonxt:
                return "IPV6_NONXT";
            case Protocol::ipv6_opts:
                return "IPV6_OPTS";
            case Protocol::any_host_internal_protocol:
                return "ANY_HOST_INTERNAL_PROTOCOL";
            case Protocol::cftp:
                return "CFTP";
            case Protocol::any_local_network:
                return "ANY_LOCAL_NETWORK";
            case Protocol::sat_expak:
                return "SAT_EXPAK";
            case Protocol::kryptolan:
                return "KRYPTOLAN";
            case Protocol::rvd:
                return "RVD";
            case Protocol::ippc:
                return "IPPC";
            case Protocol::any_distributed_file_system:
                return "ANY_DISTRIBUTED_FILE_SYSTEM";
            case Protocol::sat_mon:
                return "SAT_MON";
            case Protocol::visa:
                return "VISA";
            case Protocol::ipcv:
                return "IPCV";
            case Protocol::cpnx:
                return "CPNX";
            case Protocol::cphb:
                return "CPHB";
            case Protocol::wsn:
                return "WSN";
            case Protocol::pvp:
                return "PVP";
            case Protocol::br_sat_mon:
                return "BR_SAT_MON";
            case Protocol::sun_nd:
                return "SUN_ND";
            case Protocol::wb_mon:
                return "WB_MON";
            case Protocol::wb_expak:
                return "WB_EXPAK";
            case Protocol::iso_ip:
                return "ISO_IP";
            case Protocol::vmtp:
                return "VMTP";
            case Protocol::secure_vmtp:
                return "SECURE_VMTP";
            case Protocol::vines:
                return "VINES";
            case Protocol::ttp:  // iptm
                return "TTP or IPTM";
            case Protocol::nsfnet_igp:
                return "NSFNET_IGP";
            case Protocol::dgp:
                return "DGP";
            case Protocol::tcf:
                return "TCF";
            case Protocol::eigrp:
                return "EIGRP";
            case Protocol::ospfigp:
                return "OSPFIGP";
            case Protocol::sprite_rpc:
                return "SPRITE_RPC";
            case Protocol::larp:
                return "LARP";
            case Protocol::mtp:
                return "MTP";
            case Protocol::ax_25:
                return "AX_25";
            case Protocol::ipip:
                return "IPIP";
            case Protocol::micp:
                return "MICP";
            case Protocol::scc_sp:
                return "SCC_SP";
            case Protocol::etherip:
                return "ETHER_IP";
            case Protocol::encap:
                return "ENCAP";
            case Protocol::any_private_encryption_scheme:
                return "ANY_PRIVATE_ENCRYPTION_SCHEME";
            case Protocol::gmtp:
                return "GMTP";
            case Protocol::ifmp:
                return "IFMP";
            case Protocol::pnni:
                return "PNNI";
            case Protocol::pim:
                return "PIM";
            case Protocol::aris:
                return "ARIS";
            case Protocol::scps:
                return "SCPS";
            case Protocol::qnx:
                return "QNX";
            case Protocol::a_n:
                return "A_N";
            case Protocol::ipcomp:
                return "IPCOMP";
            case Protocol::snp:
                return "SNP";
            case Protocol::compaq_peer:
                return "COMPAQ_PEER";
            case Protocol::ipx_in_ip:
                return "IPX_IN_IP";
            case Protocol::vrrp:
                return "VRRP";
            case Protocol::pgm:
                return "PGM";
            case Protocol::any_0_hop_protocol:
                return "ANY_0_HOP_PROTOCOL";
            case Protocol::l2tp:
                return "L2TP";
            case Protocol::ddx:
                return "DDX";
            case Protocol::iatp:
                return "IATP";
            case Protocol::stp:
                return "STP";
            case Protocol::srp:
                return "SRP";
            case Protocol::uti:
                return "UTI";
            case Protocol::smp:
                return "SMP";
            case Protocol::sm:
                return "SM";
            case Protocol::ptp:
                return "PTP";
            case Protocol::isis_over_ipv4:
                return "IS_IS_OVER_IPV4";
            case Protocol::fire:
                return "FIRE";
            case Protocol::crtp:
                return "CRTP";
            case Protocol::crudp:
                return "CRUDP";
            case Protocol::sscopmce:
                return "SSCOPMCE";
            case Protocol::iplt:
                return "IPLT";
            case Protocol::sps:
                return "SPS";
            case Protocol::pipe:
                return "PIPE";
            case Protocol::sctp:
                return "SCTP";
            case Protocol::fc:
                return "FC";
            case Protocol::rsvp_e2e_ignore:
                return "RSVP_E2E_IGNORE";
            case Protocol::mobility_header:
                return "MOBILITY_HEADER";
            case Protocol::udplite:
                return "UDPLITE";
            case Protocol::mpls_in_ip:
                return "MPLS_IN_IP";
            case Protocol::manet:
                return "MANET";
            case Protocol::hip:
                return "HIP";
            case Protocol::shim6:
                return "SHIM6";
            case Protocol::wesp:
                return "WESP";
            case Protocol::rohc:
                return "ROHC";
            case Protocol::ethernet:
                return "ETHERNET";
            case Protocol::aggfrag:
                return "AGGFRAG";
            case Protocol::nsh:
                return "NSH";
            case Protocol::use_for_experimentation_and_testing1:
            case Protocol::use_for_experimentation_and_testing2:
                return "EXPERIMENT_AND_TEST_2";
            case Protocol::reserved:
                return "RESERVED";
            default:
                return "UNKNOWN_PROTOCOL";
        }
    }
}  // namespace utils::fnet::ip