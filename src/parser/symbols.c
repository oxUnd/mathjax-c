#include "parser.h"
#include <stdlib.h>
#include <string.h>

/* Symbol table: maps LaTeX command to Unicode codepoint + attributes */
typedef struct {
  const char* cmd;
  uint32_t codepoint;
  int tex_class;     /* MJX_TEXCLASS_* */
  int is_operator;   /* nonzero = has Mo-like properties */
} mjx_symbol_entry;

/* Greek letters (lowercase) */
static const mjx_symbol_entry GREEK_LOWER[] = {
  {"alpha",     0x03B1, MJX_TEXCLASS_ORD, 0},
  {"beta",      0x03B2, MJX_TEXCLASS_ORD, 0},
  {"gamma",     0x03B3, MJX_TEXCLASS_ORD, 0},
  {"delta",     0x03B4, MJX_TEXCLASS_ORD, 0},
  {"epsilon",   0x03F5, MJX_TEXCLASS_ORD, 0},
  {"varepsilon",0x03B5, MJX_TEXCLASS_ORD, 0},
  {"zeta",      0x03B6, MJX_TEXCLASS_ORD, 0},
  {"eta",       0x03B7, MJX_TEXCLASS_ORD, 0},
  {"theta",     0x03B8, MJX_TEXCLASS_ORD, 0},
  {"vartheta",  0x03D1, MJX_TEXCLASS_ORD, 0},
  {"iota",      0x03B9, MJX_TEXCLASS_ORD, 0},
  {"kappa",     0x03BA, MJX_TEXCLASS_ORD, 0},
  {"lambda",    0x03BB, MJX_TEXCLASS_ORD, 0},
  {"mu",        0x03BC, MJX_TEXCLASS_ORD, 0},
  {"nu",        0x03BD, MJX_TEXCLASS_ORD, 0},
  {"xi",        0x03BE, MJX_TEXCLASS_ORD, 0},
  {"omicron",   0x03BF, MJX_TEXCLASS_ORD, 0},
  {"pi",        0x03C0, MJX_TEXCLASS_ORD, 0},
  {"varpi",     0x03D6, MJX_TEXCLASS_ORD, 0},
  {"rho",       0x03C1, MJX_TEXCLASS_ORD, 0},
  {"varrho",    0x03F1, MJX_TEXCLASS_ORD, 0},
  {"sigma",     0x03C3, MJX_TEXCLASS_ORD, 0},
  {"varsigma",  0x03C2, MJX_TEXCLASS_ORD, 0},
  {"tau",       0x03C4, MJX_TEXCLASS_ORD, 0},
  {"upsilon",   0x03C5, MJX_TEXCLASS_ORD, 0},
  {"phi",       0x03D5, MJX_TEXCLASS_ORD, 0},
  {"varphi",    0x03C6, MJX_TEXCLASS_ORD, 0},
  {"chi",       0x03C7, MJX_TEXCLASS_ORD, 0},
  {"psi",       0x03C8, MJX_TEXCLASS_ORD, 0},
  {"omega",     0x03C9, MJX_TEXCLASS_ORD, 0},
};

/* Greek letters (uppercase) */
static const mjx_symbol_entry GREEK_UPPER[] = {
  {"Gamma",     0x0393, MJX_TEXCLASS_ORD, 0},
  {"Delta",     0x0394, MJX_TEXCLASS_ORD, 0},
  {"Theta",     0x0398, MJX_TEXCLASS_ORD, 0},
  {"Lambda",    0x039B, MJX_TEXCLASS_ORD, 0},
  {"Xi",        0x039E, MJX_TEXCLASS_ORD, 0},
  {"Pi",        0x03A0, MJX_TEXCLASS_ORD, 0},
  {"Sigma",     0x03A3, MJX_TEXCLASS_ORD, 0},
  {"Phi",       0x03A6, MJX_TEXCLASS_ORD, 0},
  {"Psi",       0x03A8, MJX_TEXCLASS_ORD, 0},
  {"Omega",     0x03A9, MJX_TEXCLASS_ORD, 0},
  {"varGamma",    0x0393, MJX_TEXCLASS_ORD, 0},
  {"varDelta",    0x0394, MJX_TEXCLASS_ORD, 0},
  {"varTheta",    0x0398, MJX_TEXCLASS_ORD, 0},
  {"varLambda",   0x039B, MJX_TEXCLASS_ORD, 0},
  {"varXi",       0x039E, MJX_TEXCLASS_ORD, 0},
  {"varPi",       0x03A0, MJX_TEXCLASS_ORD, 0},
  {"varSigma",    0x03A3, MJX_TEXCLASS_ORD, 0},
  {"varPhi",      0x03A6, MJX_TEXCLASS_ORD, 0},
  {"varPsi",      0x03A8, MJX_TEXCLASS_ORD, 0},
  {"varOmega",    0x03A9, MJX_TEXCLASS_ORD, 0},
  {"varUpsilon",  0x03A5, MJX_TEXCLASS_ORD, 0},
};

/* Binary operators */
static const mjx_symbol_entry BIN_OPS[] = {
  {"pm",         0x00B1, MJX_TEXCLASS_BIN, 0},
  {"mp",         0x2213, MJX_TEXCLASS_BIN, 0},
  {"times",      0x00D7, MJX_TEXCLASS_BIN, 0},
  {"div",        0x00F7, MJX_TEXCLASS_BIN, 0},
  {"ast",        0x2217, MJX_TEXCLASS_BIN, 0},
  {"star",       0x22C6, MJX_TEXCLASS_BIN, 0},
  {"circ",       0x2218, MJX_TEXCLASS_BIN, 0},
  {"bullet",     0x2219, MJX_TEXCLASS_BIN, 0},
  {"cdot",       0x22C5, MJX_TEXCLASS_BIN, 0},
  {"cap",        0x2229, MJX_TEXCLASS_BIN, 0},
  {"cup",        0x222A, MJX_TEXCLASS_BIN, 0},
  {"uplus",      0x228E, MJX_TEXCLASS_BIN, 0},
  {"sqcap",      0x2293, MJX_TEXCLASS_BIN, 0},
  {"sqcup",      0x2294, MJX_TEXCLASS_BIN, 0},
  {"vee",        0x2228, MJX_TEXCLASS_BIN, 0},
  {"wedge",      0x2227, MJX_TEXCLASS_BIN, 0},
  {"oplus",      0x2295, MJX_TEXCLASS_BIN, 0},
  {"ominus",     0x2296, MJX_TEXCLASS_BIN, 0},
  {"otimes",     0x2297, MJX_TEXCLASS_BIN, 0},
  {"oslash",     0x2298, MJX_TEXCLASS_BIN, 0},
  {"odot",       0x2299, MJX_TEXCLASS_BIN, 0},
  {"dagger",     0x2020, MJX_TEXCLASS_BIN, 0},
  {"ddagger",    0x2021, MJX_TEXCLASS_BIN, 0},
  {"setminus",   0x2216, MJX_TEXCLASS_BIN, 0},
  {"wr",         0x2240, MJX_TEXCLASS_BIN, 0},
  {"bigtriangleup",  0x25B3, MJX_TEXCLASS_BIN, 0},
  {"bigtriangledown",0x25BD, MJX_TEXCLASS_BIN, 0},
  {"bigcirc",    0x25EF, MJX_TEXCLASS_BIN, 0},
  {"bigstar",    0x2605, MJX_TEXCLASS_BIN, 0},
  {"barwedge",   0x22BC, MJX_TEXCLASS_BIN, 0},
  {"amalg",      0x2A3F, MJX_TEXCLASS_BIN, 0},
  {"dotplus",    0x2214, MJX_TEXCLASS_BIN, 0},
  {"ltimes",     0x22C9, MJX_TEXCLASS_BIN, 0},
  {"rtimes",     0x22CA, MJX_TEXCLASS_BIN, 0},
  {"leftthreetimes", 0x22CB, MJX_TEXCLASS_BIN, 0},
  {"rightthreetimes",0x22CC, MJX_TEXCLASS_BIN, 0},
  {"curlywedge", 0x22CF, MJX_TEXCLASS_BIN, 0},
  {"curlyvee",   0x22CE, MJX_TEXCLASS_BIN, 0},
  {"veebar",     0x22BB, MJX_TEXCLASS_BIN, 0},
  {"boxminus",   0x229F, MJX_TEXCLASS_BIN, 0},
  {"boxplus",    0x229E, MJX_TEXCLASS_BIN, 0},
  {"boxtimes",   0x22A0, MJX_TEXCLASS_BIN, 0},
  {"boxdot",     0x22A1, MJX_TEXCLASS_BIN, 0},
  {"circledast", 0x229B, MJX_TEXCLASS_BIN, 0},
  {"circledcirc",0x229A, MJX_TEXCLASS_BIN, 0},
  {"circleddash",0x229D, MJX_TEXCLASS_BIN, 0},
  {"intercal",   0x22BA, MJX_TEXCLASS_BIN, 0},
  {"doublebarwedge", 0x2A5E, MJX_TEXCLASS_BIN, 0},
  {"divideontimes",  0x22C7, MJX_TEXCLASS_BIN, 0},
  {"And",        0x0026, MJX_TEXCLASS_BIN, 0},
  {"land",       0x2227, MJX_TEXCLASS_BIN, 0},
  {"lor",        0x2228, MJX_TEXCLASS_BIN, 0},
  {"Cap",        0x22D2, MJX_TEXCLASS_BIN, 0},
  {"Cup",        0x22D3, MJX_TEXCLASS_BIN, 0},
  {"doublecap",  0x22D2, MJX_TEXCLASS_BIN, 0},
  {"doublecup",  0x22D3, MJX_TEXCLASS_BIN, 0},
};

/* Relations */
static const mjx_symbol_entry RELATIONS[] = {
  {"le",         0x2264, MJX_TEXCLASS_REL, 0},
  {"leq",        0x2264, MJX_TEXCLASS_REL, 0},
  {"ge",         0x2265, MJX_TEXCLASS_REL, 0},
  {"geq",        0x2265, MJX_TEXCLASS_REL, 0},
  {"ne",         0x2260, MJX_TEXCLASS_REL, 0},
  {"neq",        0x2260, MJX_TEXCLASS_REL, 0},
  {"sim",        0x223C, MJX_TEXCLASS_REL, 0},
  {"simeq",      0x2243, MJX_TEXCLASS_REL, 0},
  {"approx",     0x2248, MJX_TEXCLASS_REL, 0},
  {"approxeq",   0x224A, MJX_TEXCLASS_REL, 0},
  {"cong",       0x2245, MJX_TEXCLASS_REL, 0},
  {"equiv",      0x2261, MJX_TEXCLASS_REL, 0},
  {"doteq",      0x2250, MJX_TEXCLASS_REL, 0},
  {"doteqdot",   0x2251, MJX_TEXCLASS_REL, 0},
  {"asymp",      0x224D, MJX_TEXCLASS_REL, 0},
  {"propto",     0x221D, MJX_TEXCLASS_REL, 0},
  {"models",     0x22A7, MJX_TEXCLASS_REL, 0},
  {"perp",       0x22A5, MJX_TEXCLASS_REL, 0},
  {"mid",        0x2223, MJX_TEXCLASS_REL, 0},
  {"nmid",       0x2224, MJX_TEXCLASS_REL, 0},
  {"parallel",   0x2225, MJX_TEXCLASS_REL, 0},
  {"nparallel",  0x2226, MJX_TEXCLASS_REL, 0},
  {"bowtie",     0x22C8, MJX_TEXCLASS_REL, 0},
  {"smile",      0x2323, MJX_TEXCLASS_REL, 0},
  {"frown",      0x2322, MJX_TEXCLASS_REL, 0},
  {"dashv",      0x22A3, MJX_TEXCLASS_REL, 0},
  {"vdash",      0x22A2, MJX_TEXCLASS_REL, 0},
  {"vDash",      0x22A8, MJX_TEXCLASS_REL, 0},
  {"Vdash",      0x22A9, MJX_TEXCLASS_REL, 0},
  {"Vvdash",     0x22AA, MJX_TEXCLASS_REL, 0},
  {"subset",     0x2282, MJX_TEXCLASS_REL, 0},
  {"supset",     0x2283, MJX_TEXCLASS_REL, 0},
  {"subseteq",   0x2286, MJX_TEXCLASS_REL, 0},
  {"supseteq",   0x2287, MJX_TEXCLASS_REL, 0},
  {"subsetneq",  0x228A, MJX_TEXCLASS_REL, 0},
  {"supsetneq",  0x228B, MJX_TEXCLASS_REL, 0},
  {"sqsubset",   0x228F, MJX_TEXCLASS_REL, 0},
  {"sqsupset",   0x2290, MJX_TEXCLASS_REL, 0},
  {"sqsubseteq", 0x2291, MJX_TEXCLASS_REL, 0},
  {"sqsupseteq", 0x2292, MJX_TEXCLASS_REL, 0},
  {"in",         0x2208, MJX_TEXCLASS_REL, 0},
  {"ni",         0x220B, MJX_TEXCLASS_REL, 0},
  {"owns",       0x220B, MJX_TEXCLASS_REL, 0},
  {"notin",      0x2209, MJX_TEXCLASS_REL, 0},
  {"prec",       0x227A, MJX_TEXCLASS_REL, 0},
  {"succ",       0x227B, MJX_TEXCLASS_REL, 0},
  {"preceq",     0x227C, MJX_TEXCLASS_REL, 0},
  {"succeq",     0x227D, MJX_TEXCLASS_REL, 0},
  {"ll",         0x226A, MJX_TEXCLASS_REL, 0},
  {"gg",         0x226B, MJX_TEXCLASS_REL, 0},
  {"between",    0x226C, MJX_TEXCLASS_REL, 0},
  {"pitchfork",  0x22D4, MJX_TEXCLASS_REL, 0},
  {"therefore",  0x2234, MJX_TEXCLASS_REL, 0},
  {"because",    0x2235, MJX_TEXCLASS_REL, 0},
  {"vartriangle",      0x25B3, MJX_TEXCLASS_REL, 0},
  {"triangleright",    0x25B7, MJX_TEXCLASS_REL, 0},
  {"triangleleft",     0x25C1, MJX_TEXCLASS_REL, 0},
  {"vartriangleright", 0x22B3, MJX_TEXCLASS_REL, 0},
  {"vartriangleleft",  0x22B2, MJX_TEXCLASS_REL, 0},
  {"trianglelefteq",   0x22B4, MJX_TEXCLASS_REL, 0},
  {"trianglerighteq",  0x22B5, MJX_TEXCLASS_REL, 0},
  {"backsim",    0x223D, MJX_TEXCLASS_REL, 0},
  {"backsimeq",  0x22CD, MJX_TEXCLASS_REL, 0},
  {"eqsim",      0x2242, MJX_TEXCLASS_REL, 0},
  {"eqcirc",     0x2256, MJX_TEXCLASS_REL, 0},
  {"fallingdotseq", 0x2252, MJX_TEXCLASS_REL, 0},
  {"risingdotseq",  0x2253, MJX_TEXCLASS_REL, 0},
  {"bumpeq",     0x224F, MJX_TEXCLASS_REL, 0},
  {"Bumpeq",     0x224E, MJX_TEXCLASS_REL, 0},
  {"to",         0x2192, MJX_TEXCLASS_REL, 0},
  {"gets",       0x2190, MJX_TEXCLASS_REL, 0},
  {"leftarrow",  0x2190, MJX_TEXCLASS_REL, 0},
  {"rightarrow", 0x2192, MJX_TEXCLASS_REL, 0},
  {"Leftarrow",  0x21D0, MJX_TEXCLASS_REL, 0},
  {"Rightarrow", 0x21D2, MJX_TEXCLASS_REL, 0},
  {"leftrightarrow",   0x2194, MJX_TEXCLASS_REL, 0},
  {"Leftrightarrow",   0x21D4, MJX_TEXCLASS_REL, 0},
  {"mapsto",     0x21A6, MJX_TEXCLASS_REL, 0},
  {"longmapsto", 0x27FC, MJX_TEXCLASS_REL, 0},
  {"hookrightarrow",  0x21AA, MJX_TEXCLASS_REL, 0},
  {"hookleftarrow",   0x21A9, MJX_TEXCLASS_REL, 0},
  {"twoheadrightarrow", 0x21A0, MJX_TEXCLASS_REL, 0},
  {"twoheadleftarrow",  0x219E, MJX_TEXCLASS_REL, 0},
  {"rightleftharpoons", 0x21CC, MJX_TEXCLASS_REL, 0},
  {"leftrightharpoons", 0x21CB, MJX_TEXCLASS_REL, 0},
  {"rightharpoonup", 0x21C0, MJX_TEXCLASS_REL, 0},
  {"rightharpoondown", 0x21C1, MJX_TEXCLASS_REL, 0},
  {"leftharpoonup", 0x21BC, MJX_TEXCLASS_REL, 0},
  {"leftharpoondown", 0x21BD, MJX_TEXCLASS_REL, 0},
  {"nearrow",    0x2197, MJX_TEXCLASS_REL, 0},
  {"searrow",    0x2198, MJX_TEXCLASS_REL, 0},
  {"swarrow",    0x2199, MJX_TEXCLASS_REL, 0},
  {"nwarrow",    0x2196, MJX_TEXCLASS_REL, 0},
  {"uparrow",    0x2191, MJX_TEXCLASS_REL, 0},
  {"downarrow",  0x2193, MJX_TEXCLASS_REL, 0},
  {"updownarrow", 0x2195, MJX_TEXCLASS_REL, 0},
  {"Uparrow",    0x21D1, MJX_TEXCLASS_REL, 0},
  {"Downarrow",  0x21D3, MJX_TEXCLASS_REL, 0},
  {"Updownarrow", 0x21D5, MJX_TEXCLASS_REL, 0},
  {"longleftrightarrow", 0x27F7, MJX_TEXCLASS_REL, 0},
  {"Longleftrightarrow", 0x27FA, MJX_TEXCLASS_REL, 0},
  {"longleftarrow",   0x27F5, MJX_TEXCLASS_REL, 0},
  {"longrightarrow",  0x27F6, MJX_TEXCLASS_REL, 0},
  {"Longleftarrow",   0x27F8, MJX_TEXCLASS_REL, 0},
  {"Longrightarrow",  0x27F9, MJX_TEXCLASS_REL, 0},
  {"lll",        0x22D8, MJX_TEXCLASS_REL, 0},
  {"ggg",        0x22D9, MJX_TEXCLASS_REL, 0},
  {"Join",       0x2A1D, MJX_TEXCLASS_REL, 0},
  {"leqq",       0x2266, MJX_TEXCLASS_REL, 0},
  {"geqq",       0x2267, MJX_TEXCLASS_REL, 0},
  {"leqslant",   0x2A7D, MJX_TEXCLASS_REL, 0},
  {"geqslant",   0x2A7E, MJX_TEXCLASS_REL, 0},
  {"nless",      0x226E, MJX_TEXCLASS_REL, 0},
  {"ngtr",       0x226F, MJX_TEXCLASS_REL, 0},
  {"nleq",       0x2270, MJX_TEXCLASS_REL, 0},
  {"ngeq",       0x2271, MJX_TEXCLASS_REL, 0},
  {"nleqslant",  0x2A87, MJX_TEXCLASS_REL, 0},
  {"ngeqslant",  0x2A88, MJX_TEXCLASS_REL, 0},
  {"nleqq",      0x2270, MJX_TEXCLASS_REL, 0},
  {"ngeqq",      0x2271, MJX_TEXCLASS_REL, 0},
  {"lneq",       0x2A87, MJX_TEXCLASS_REL, 0},
  {"gneq",       0x2A88, MJX_TEXCLASS_REL, 0},
  {"lneqq",      0x2268, MJX_TEXCLASS_REL, 0},
  {"gneqq",      0x2269, MJX_TEXCLASS_REL, 0},
  {"lnsim",      0x22E6, MJX_TEXCLASS_REL, 0},
  {"gnsim",      0x22E7, MJX_TEXCLASS_REL, 0},
  {"lnapprox",   0x2A89, MJX_TEXCLASS_REL, 0},
  {"gnapprox",   0x2A8A, MJX_TEXCLASS_REL, 0},
  {"nprec",      0x2280, MJX_TEXCLASS_REL, 0},
  {"nsucc",      0x2281, MJX_TEXCLASS_REL, 0},
  {"npreceq",    0x22E0, MJX_TEXCLASS_REL, 0},
  {"nsucceq",    0x22E1, MJX_TEXCLASS_REL, 0},
  {"precneqq",   0x2AB5, MJX_TEXCLASS_REL, 0},
  {"succneqq",   0x2AB6, MJX_TEXCLASS_REL, 0},
  {"precnsim",   0x22E8, MJX_TEXCLASS_REL, 0},
  {"succnsim",   0x22E9, MJX_TEXCLASS_REL, 0},
  {"precnapprox",0x2AB9, MJX_TEXCLASS_REL, 0},
  {"succnapprox",0x2ABA, MJX_TEXCLASS_REL, 0},
  {"nsim",       0x2241, MJX_TEXCLASS_REL, 0},
  {"ncong",      0x2247, MJX_TEXCLASS_REL, 0},
  {"nvdash",     0x22AC, MJX_TEXCLASS_REL, 0},
  {"nvDash",     0x22AD, MJX_TEXCLASS_REL, 0},
  {"nVdash",     0x22AE, MJX_TEXCLASS_REL, 0},
  {"nVDash",     0x22AF, MJX_TEXCLASS_REL, 0},
  {"ntriangleleft", 0x22EA, MJX_TEXCLASS_REL, 0},
  {"ntriangleright",0x22EB, MJX_TEXCLASS_REL, 0},
  {"ntrianglelefteq", 0x22EC, MJX_TEXCLASS_REL, 0},
  {"ntrianglerighteq",0x22ED, MJX_TEXCLASS_REL, 0},
  {"nsubseteq",  0x2288, MJX_TEXCLASS_REL, 0},
  {"nsupseteq",  0x2289, MJX_TEXCLASS_REL, 0},
  {"nsubseteqq", 0x2288, MJX_TEXCLASS_REL, 0},
  {"nsupseteqq", 0x2289, MJX_TEXCLASS_REL, 0},
  {"subsetneqq", 0x2ACB, MJX_TEXCLASS_REL, 0},
  {"supsetneqq", 0x2ACC, MJX_TEXCLASS_REL, 0},
  {"lhd",        0x22B2, MJX_TEXCLASS_REL, 0},
  {"rhd",        0x22B3, MJX_TEXCLASS_REL, 0},
  {"unlhd",      0x22B4, MJX_TEXCLASS_REL, 0},
  {"unrhd",      0x22B5, MJX_TEXCLASS_REL, 0},
  {"Subset",     0x22D0, MJX_TEXCLASS_REL, 0},
  {"Supset",     0x22D1, MJX_TEXCLASS_REL, 0},
  {"subseteqq",  0x2AC5, MJX_TEXCLASS_REL, 0},
  {"supseteqq",  0x2AC6, MJX_TEXCLASS_REL, 0},
  {"lessdot",    0x22D6, MJX_TEXCLASS_REL, 0},
  {"gtrdot",     0x22D7, MJX_TEXCLASS_REL, 0},
  {"lesssim",    0x2272, MJX_TEXCLASS_REL, 0},
  {"gtrsim",     0x2273, MJX_TEXCLASS_REL, 0},
  {"lessgtr",    0x2276, MJX_TEXCLASS_REL, 0},
  {"gtrless",    0x2277, MJX_TEXCLASS_REL, 0},
  {"lesseqgtr",  0x22DA, MJX_TEXCLASS_REL, 0},
  {"gtreqless",  0x22DB, MJX_TEXCLASS_REL, 0},
  {"lesseqqgtr", 0x2A8B, MJX_TEXCLASS_REL, 0},
  {"gtreqqless", 0x2A8C, MJX_TEXCLASS_REL, 0},
  {"lessapprox", 0x2A85, MJX_TEXCLASS_REL, 0},
  {"gtrapprox",  0x2A86, MJX_TEXCLASS_REL, 0},
  {"eqslantless",0x2A95, MJX_TEXCLASS_REL, 0},
  {"eqslantgtr", 0x2A96, MJX_TEXCLASS_REL, 0},
  {"precsim",    0x227E, MJX_TEXCLASS_REL, 0},
  {"succsim",    0x227F, MJX_TEXCLASS_REL, 0},
  {"precapprox", 0x2AB7, MJX_TEXCLASS_REL, 0},
  {"succapprox", 0x2AB8, MJX_TEXCLASS_REL, 0},
  {"preccurlyeq",0x227C, MJX_TEXCLASS_REL, 0},
  {"succcurlyeq",0x227D, MJX_TEXCLASS_REL, 0},
  {"curlyeqprec",0x22DE, MJX_TEXCLASS_REL, 0},
  {"curlyeqsucc",0x22DF, MJX_TEXCLASS_REL, 0},
  {"multimap",   0x22B8, MJX_TEXCLASS_REL, 0},
  {"circeq",     0x2257, MJX_TEXCLASS_REL, 0},
  {"thickapprox",0x2248, MJX_TEXCLASS_REL, 0},
  {"napprox",    0x2249, MJX_TEXCLASS_REL, 0},
  {"dashleftarrow", 0x21E0, MJX_TEXCLASS_REL, 0},
  {"dashrightarrow",0x21E2, MJX_TEXCLASS_REL, 0},
  {"leftleftarrows",0x21C7, MJX_TEXCLASS_REL, 0},
  {"rightrightarrows",0x21C9, MJX_TEXCLASS_REL, 0},
  {"leftrightarrows",0x21C6, MJX_TEXCLASS_REL, 0},
  {"rightleftarrows",0x21C4, MJX_TEXCLASS_REL, 0},
  {"Lleftarrow", 0x21DA, MJX_TEXCLASS_REL, 0},
  {"Rrightarrow",0x21DB, MJX_TEXCLASS_REL, 0},
  {"leftarrowtail",0x21A2, MJX_TEXCLASS_REL, 0},
  {"rightarrowtail",0x21A3, MJX_TEXCLASS_REL, 0},
  {"Lsh",        0x21B0, MJX_TEXCLASS_REL, 0},
  {"Rsh",        0x21B1, MJX_TEXCLASS_REL, 0},
  {"looparrowleft",0x21AB, MJX_TEXCLASS_REL, 0},
  {"looparrowright",0x21AC, MJX_TEXCLASS_REL, 0},
  {"curvearrowleft",0x21B6, MJX_TEXCLASS_REL, 0},
  {"curvearrowright",0x21B7, MJX_TEXCLASS_REL, 0},
  {"circlearrowleft",0x21BA, MJX_TEXCLASS_REL, 0},
  {"circlearrowright",0x21BB, MJX_TEXCLASS_REL, 0},
  {"nleftarrow", 0x219A, MJX_TEXCLASS_REL, 0},
  {"nrightarrow",0x219B, MJX_TEXCLASS_REL, 0},
  {"nLeftarrow", 0x21CD, MJX_TEXCLASS_REL, 0},
  {"nRightarrow",0x21CF, MJX_TEXCLASS_REL, 0},
  {"nleftrightarrow",0x21AE, MJX_TEXCLASS_REL, 0},
  {"nLeftrightarrow",0x21CE, MJX_TEXCLASS_REL, 0},
  {"upuparrows", 0x21C8, MJX_TEXCLASS_REL, 0},
  {"downdownarrows",0x21CA, MJX_TEXCLASS_REL, 0},
  {"downharpoonleft",0x21C3, MJX_TEXCLASS_REL, 0},
  {"downharpoonright",0x21C2, MJX_TEXCLASS_REL, 0},
  {"upharpoonleft",0x21BF, MJX_TEXCLASS_REL, 0},
  {"upharpoonright",0x21BE, MJX_TEXCLASS_REL, 0},
  {"rightsquigarrow",0x21DD, MJX_TEXCLASS_REL, 0},
  {"leftrightsquigarrow",0x21AD, MJX_TEXCLASS_REL, 0},
  {"leadsto",    0x21DD, MJX_TEXCLASS_REL, 0},
};

/* Large operators */
static const mjx_symbol_entry LARGE_OPS[] = {
  {"sum",        0x2211, MJX_TEXCLASS_OP, 1},
  {"prod",       0x220F, MJX_TEXCLASS_OP, 1},
  {"coprod",     0x2210, MJX_TEXCLASS_OP, 1},
  {"int",        0x222B, MJX_TEXCLASS_OP, 1},
  {"iint",       0x222C, MJX_TEXCLASS_OP, 1},
  {"iiint",      0x222D, MJX_TEXCLASS_OP, 1},
  {"iiiint",     0x2A0C, MJX_TEXCLASS_OP, 1},
  {"oint",       0x222E, MJX_TEXCLASS_OP, 1},
  {"smallint",   0x222B, MJX_TEXCLASS_OP, 0},
  {"bigcap",     0x22C2, MJX_TEXCLASS_OP, 1},
  {"bigcup",     0x22C3, MJX_TEXCLASS_OP, 1},
  {"bigsqcup",   0x2A06, MJX_TEXCLASS_OP, 1},
  {"bigvee",     0x22C1, MJX_TEXCLASS_OP, 1},
  {"bigwedge",   0x22C0, MJX_TEXCLASS_OP, 1},
  {"bigodot",    0x2A00, MJX_TEXCLASS_OP, 1},
  {"bigotimes",  0x2A02, MJX_TEXCLASS_OP, 1},
  {"bigoplus",   0x2A01, MJX_TEXCLASS_OP, 1},
  {"biguplus",   0x2A04, MJX_TEXCLASS_OP, 1},
  {"oiint",      0x222F, MJX_TEXCLASS_OP, 1},
  {"oiiint",     0x2230, MJX_TEXCLASS_OP, 1},
  {"lim",        0,      MJX_TEXCLASS_OP, 0},
  {"limsup",     0,      MJX_TEXCLASS_OP, 0},
  {"liminf",     0,      MJX_TEXCLASS_OP, 0},
  {"varlimsup",  0,      MJX_TEXCLASS_OP, 0},
  {"varliminf",  0,      MJX_TEXCLASS_OP, 0},
  {"injlim",     0,      MJX_TEXCLASS_OP, 0},
  {"projlim",    0,      MJX_TEXCLASS_OP, 0},
  {"varinjlim",  0,      MJX_TEXCLASS_OP, 0},
  {"varprojlim", 0,      MJX_TEXCLASS_OP, 0},
  {"max",        0,      MJX_TEXCLASS_OP, 0},
  {"min",        0,      MJX_TEXCLASS_OP, 0},
  {"sup",        0,      MJX_TEXCLASS_OP, 0},
  {"inf",        0,      MJX_TEXCLASS_OP, 0},
  {"det",        0,      MJX_TEXCLASS_OP, 0},
  {"Pr",         0,      MJX_TEXCLASS_OP, 0},
  {"gcd",        0,      MJX_TEXCLASS_OP, 0},
  {"deg",        0,      MJX_TEXCLASS_OP, 0},
  {"arg",        0,      MJX_TEXCLASS_OP, 0},
  {"dim",        0,      MJX_TEXCLASS_OP, 0},
  {"hom",        0,      MJX_TEXCLASS_OP, 0},
  {"ker",        0,      MJX_TEXCLASS_OP, 0},
  {"lg",         0,      MJX_TEXCLASS_OP, 0},
  {"ln",         0,      MJX_TEXCLASS_OP, 0},
  {"log",        0,      MJX_TEXCLASS_OP, 0},
  {"exp",        0,      MJX_TEXCLASS_OP, 0},
  {"sin",        0,      MJX_TEXCLASS_OP, 0},
  {"cos",        0,      MJX_TEXCLASS_OP, 0},
  {"tan",        0,      MJX_TEXCLASS_OP, 0},
  {"cot",        0,      MJX_TEXCLASS_OP, 0},
  {"sec",        0,      MJX_TEXCLASS_OP, 0},
  {"csc",        0,      MJX_TEXCLASS_OP, 0},
  {"sinh",       0,      MJX_TEXCLASS_OP, 0},
  {"cosh",       0,      MJX_TEXCLASS_OP, 0},
  {"tanh",       0,      MJX_TEXCLASS_OP, 0},
  {"coth",       0,      MJX_TEXCLASS_OP, 0},
  {"arcsin",     0,      MJX_TEXCLASS_OP, 0},
  {"arccos",     0,      MJX_TEXCLASS_OP, 0},
  {"arctan",     0,      MJX_TEXCLASS_OP, 0},
};

/* Miscellaneous symbols */
static const mjx_symbol_entry MISC_SYMBOLS[] = {
  {"aleph",      0x2135, MJX_TEXCLASS_ORD, 0},
  {"hbar",       0x210F, MJX_TEXCLASS_ORD, 0},
  {"hslash",     0x210F, MJX_TEXCLASS_ORD, 0},
  {"imath",      0x0131, MJX_TEXCLASS_ORD, 0},
  {"jmath",      0x0237, MJX_TEXCLASS_ORD, 0},
  {"ell",        0x2113, MJX_TEXCLASS_ORD, 0},
  {"wp",         0x2118, MJX_TEXCLASS_ORD, 0},
  {"Re",         0x211C, MJX_TEXCLASS_ORD, 0},
  {"Im",         0x2111, MJX_TEXCLASS_ORD, 0},
  {"partial",    0x2202, MJX_TEXCLASS_ORD, 0},
  {"infty",      0x221E, MJX_TEXCLASS_ORD, 0},
  {"prime",      0x2032, MJX_TEXCLASS_ORD, 0},
  {"emptyset",   0x2205, MJX_TEXCLASS_ORD, 0},
  {"nabla",      0x2207, MJX_TEXCLASS_ORD, 0},
  {"surd",       0x221A, MJX_TEXCLASS_ORD, 0},
  {"angle",      0x2220, MJX_TEXCLASS_ORD, 0},
  {"measuredangle", 0x2221, MJX_TEXCLASS_ORD, 0},
  {"triangle",   0x25B3, MJX_TEXCLASS_ORD, 0},
  {"forall",     0x2200, MJX_TEXCLASS_ORD, 0},
  {"exists",     0x2203, MJX_TEXCLASS_ORD, 0},
  {"nexists",    0x2204, MJX_TEXCLASS_ORD, 0},
  {"neg",        0x00AC, MJX_TEXCLASS_ORD, 0},
  {"lnot",       0x00AC, MJX_TEXCLASS_ORD, 0},
  {"top",        0x22A4, MJX_TEXCLASS_ORD, 0},
  {"bot",        0x22A5, MJX_TEXCLASS_ORD, 0},
  {"Box",        0x25A1, MJX_TEXCLASS_ORD, 0},
  {"Diamond",    0x25C7, MJX_TEXCLASS_ORD, 0},
  {"clubsuit",   0x2663, MJX_TEXCLASS_ORD, 0},
  {"diamondsuit",0x2662, MJX_TEXCLASS_ORD, 0},
  {"heartsuit",  0x2661, MJX_TEXCLASS_ORD, 0},
  {"spadesuit",  0x2660, MJX_TEXCLASS_ORD, 0},
  {"backepsilon",  0x220D, MJX_TEXCLASS_ORD, 0},
  {"digamma",      0x03DD, MJX_TEXCLASS_ORD, 0},
  {"Bbbk",         0x1D55C, MJX_TEXCLASS_ORD, 0},
  {"dagger",       0x2020, MJX_TEXCLASS_ORD, 0},
  {"ddagger",      0x2021, MJX_TEXCLASS_ORD, 0},
  {"blacksquare",  0x25A0, MJX_TEXCLASS_ORD, 0},
  {"blacktriangle", 0x25B4, MJX_TEXCLASS_ORD, 0},
  {"blacktriangledown", 0x25BE, MJX_TEXCLASS_ORD, 0},
  {"lozenge",      0x25CA, MJX_TEXCLASS_ORD, 0},
  {"square",       0x25A1, MJX_TEXCLASS_ORD, 0},
  {"circledR",     0x00AE, MJX_TEXCLASS_ORD, 0},
  {"circledS",     0x24C8, MJX_TEXCLASS_ORD, 0},
  {"checkmark",    0x2713, MJX_TEXCLASS_ORD, 0},
  {"maltese",      0x2720, MJX_TEXCLASS_ORD, 0},
  {"Finv",         0x2132, MJX_TEXCLASS_ORD, 0},
  {"Game",         0x2141, MJX_TEXCLASS_ORD, 0},
  {"beth",         0x2136, MJX_TEXCLASS_ORD, 0},
  {"gimel",        0x2137, MJX_TEXCLASS_ORD, 0},
  {"daleth",       0x2138, MJX_TEXCLASS_ORD, 0},
  {"sphericalangle", 0x2222, MJX_TEXCLASS_ORD, 0},
  {"blacklozenge", 0x29EB, MJX_TEXCLASS_ORD, 0},
  {"blacktriangleleft", 0x25C0, MJX_TEXCLASS_ORD, 0},
  {"blacktriangleright",0x25B6, MJX_TEXCLASS_ORD, 0},
  {"backprime",    0x2035, MJX_TEXCLASS_ORD, 0},
  {"diagdown",     0x27CD, MJX_TEXCLASS_ORD, 0},
  {"diagup",       0x27CB, MJX_TEXCLASS_ORD, 0},
  {"eth",          0x00F0, MJX_TEXCLASS_ORD, 0},
  {"flat",         0x266D, MJX_TEXCLASS_ORD, 0},
  {"natural",      0x266E, MJX_TEXCLASS_ORD, 0},
  {"sharp",        0x266F, MJX_TEXCLASS_ORD, 0},
  {"mho",          0x2127, MJX_TEXCLASS_ORD, 0},
  {"complement",   0x2201, MJX_TEXCLASS_ORD, 0},
  {"copyright",    0x00A9, MJX_TEXCLASS_ORD, 0},
  {"pounds",       0x00A3, MJX_TEXCLASS_ORD, 0},
  {"yen",          0x00A5, MJX_TEXCLASS_ORD, 0},
  {"triangleq",    0x225C, MJX_TEXCLASS_REL, 0},
  {"triangledown", 0x25BD, MJX_TEXCLASS_ORD, 0},
  {"varkappa",     0x03F0, MJX_TEXCLASS_ORD, 0},
};

/* Spacing commands */
static const mjx_symbol_entry SPACING[] = {
  {",",  0, MJX_TEXCLASS_NONE, 0},
  {":",  0, MJX_TEXCLASS_NONE, 0},
  {";",  0, MJX_TEXCLASS_NONE, 0},
  {"!",  0, MJX_TEXCLASS_NONE, 0},
  {" ",  0, MJX_TEXCLASS_NONE, 0},
  {"quad", 0, MJX_TEXCLASS_NONE, 0},
  {"qquad", 0, MJX_TEXCLASS_NONE, 0},
};

/* Dots */
static const mjx_symbol_entry DOTS[] = {
  {"dots",       0x2026, MJX_TEXCLASS_ORD, 0},
  {"ldots",      0x2026, MJX_TEXCLASS_ORD, 0},
  {"cdots",      0x22EF, MJX_TEXCLASS_ORD, 0},
  {"dotsb",      0x22EF, MJX_TEXCLASS_ORD, 0},
  {"dotsc",      0x2026, MJX_TEXCLASS_ORD, 0},
  {"dotsi",      0x22EF, MJX_TEXCLASS_ORD, 0},
  {"dotsm",      0x22EF, MJX_TEXCLASS_ORD, 0},
  {"dotso",      0x2026, MJX_TEXCLASS_ORD, 0},
  {"vdots",      0x22EE, MJX_TEXCLASS_ORD, 0},
  {"ddots",      0x22F1, MJX_TEXCLASS_ORD, 0},
};

/* Accents */
static const mjx_symbol_entry ACCENTS[] = {
  {"hat",        0x0302, MJX_TEXCLASS_ORD, 0},
  {"tilde",      0x0303, MJX_TEXCLASS_ORD, 0},
  {"bar",        0x0304, MJX_TEXCLASS_ORD, 0},
  {"dot",        0x0307, MJX_TEXCLASS_ORD, 0},
  {"ddot",       0x0308, MJX_TEXCLASS_ORD, 0},
  {"breve",      0x0306, MJX_TEXCLASS_ORD, 0},
  {"check",      0x030C, MJX_TEXCLASS_ORD, 0},
  {"vec",        0x20D7, MJX_TEXCLASS_ORD, 0},
  {"acute",      0x0301, MJX_TEXCLASS_ORD, 0},
  {"grave",      0x0300, MJX_TEXCLASS_ORD, 0},
  {"mathring",   0x030A, MJX_TEXCLASS_ORD, 0},
  {"widehat",    0x0302, MJX_TEXCLASS_ORD, 0},
  {"widetilde",  0x0303, MJX_TEXCLASS_ORD, 0},
};

/* Delimiters */
typedef struct {
  const char* name;
  const char* unicode;
} mjx_delimiter_entry;

static const mjx_delimiter_entry DELIMITERS[] = {
  {"(", "("},
  {")", ")"},
  {"[", "["},
  {"]", "]"},
  {"{", "{"},
  {"}", "}"},
  {"langle", "\xE2\x9F\xA8"},
  {"rangle", "\xE2\x9F\xA9"},
  {"lbrace", "{"},
  {"rbrace", "}"},
  {"lbrack", "["},
  {"rbrack", "]"},
  {"lparen", "("},
  {"rparen", ")"},
  {"lVert",  "\xE2\x80\x96"},
  {"rVert",  "\xE2\x80\x96"},
  {"lvert",  "|"},
  {"rvert",  "|"},
  {"lgroup", "\xE2\x8E\x96"},
  {"rgroup", "\xE2\x8E\x97"},
  {"lfloor", "\xE2\x8C\x8A"},
  {"rfloor", "\xE2\x8C\x8B"},
  {"lceil",  "\xE2\x8C\x88"},
  {"rceil",  "\xE2\x8C\x89"},
  {"ulcorner", "\xE2\x8C\x9C"},
  {"urcorner", "\xE2\x8C\x9D"},
  {"llcorner", "\xE2\x8C\x9E"},
  {"lrcorner", "\xE2\x8C\x9F"},
  {"arrowvert", "|"},
  {"Arrowvert", "\xE2\x80\x96"},
  {"bracevert", "|"},
  {"|", "|"},
  {"\\|", "\xE2\x80\x96"},
  {".", ""},
  {"backslash", "\\"},
  {"uparrow", "\xE2\x86\x91"},
  {"downarrow", "\xE2\x86\x93"},
  {"updownarrow", "\xE2\x86\x95"},
  {"Uparrow", "\xE2\x87\x91"},
  {"Downarrow", "\xE2\x87\x93"},
  {"Updownarrow", "\xE2\x87\x95"},
  {"vert", "|"},
  {"Vert", "\xE2\x80\x96"},
  {"lmoustache",  "\xE2\x8E\xB0"},
  {"rmoustache",  "\xE2\x8E\xB1"},
};

/* Lookup a symbol in a table */
static const mjx_symbol_entry* lookup_symbol(const mjx_symbol_entry* table, size_t count,
                                              const char* name) {
  for (size_t i = 0; i < count; i++) {
    if (strcmp(table[i].cmd, name) == 0) return &table[i];
  }
  return NULL;
}

/* Public API to look up a LaTeX command */
int mjx_lookup_greek(const char* cmd, uint32_t* cp, int* tex_class) {
  const mjx_symbol_entry* e;
  e = lookup_symbol(GREEK_LOWER, sizeof(GREEK_LOWER)/sizeof(GREEK_LOWER[0]), cmd);
  if (!e) e = lookup_symbol(GREEK_UPPER, sizeof(GREEK_UPPER)/sizeof(GREEK_UPPER[0]), cmd);
  if (e) { *cp = e->codepoint; *tex_class = e->tex_class; return 1; }
  return 0;
}

int mjx_lookup_binop(const char* cmd, uint32_t* cp, int* tex_class) {
  const mjx_symbol_entry* e = lookup_symbol(BIN_OPS, sizeof(BIN_OPS)/sizeof(BIN_OPS[0]), cmd);
  if (e) { *cp = e->codepoint; *tex_class = e->tex_class; return 1; }
  return 0;
}

int mjx_lookup_relation(const char* cmd, uint32_t* cp, int* tex_class) {
  const mjx_symbol_entry* e = lookup_symbol(RELATIONS, sizeof(RELATIONS)/sizeof(RELATIONS[0]), cmd);
  if (e) { *cp = e->codepoint; *tex_class = e->tex_class; return 1; }
  return 0;
}

int mjx_lookup_largeop(const char* cmd, uint32_t* cp, int* tex_class, int* is_operator) {
  const mjx_symbol_entry* e = lookup_symbol(LARGE_OPS, sizeof(LARGE_OPS)/sizeof(LARGE_OPS[0]), cmd);
  if (e) { *cp = e->codepoint; *tex_class = e->tex_class; *is_operator = e->is_operator; return 1; }
  return 0;
}

int mjx_lookup_misc(const char* cmd, uint32_t* cp, int* tex_class) {
  const mjx_symbol_entry* e = lookup_symbol(MISC_SYMBOLS, sizeof(MISC_SYMBOLS)/sizeof(MISC_SYMBOLS[0]), cmd);
  if (!e) e = lookup_symbol(DOTS, sizeof(DOTS)/sizeof(DOTS[0]), cmd);
  if (e) { *cp = e->codepoint; *tex_class = e->tex_class; return 1; }
  return 0;
}

int mjx_lookup_accent(const char* cmd, uint32_t* cp) {
  const mjx_symbol_entry* e = lookup_symbol(ACCENTS, sizeof(ACCENTS)/sizeof(ACCENTS[0]), cmd);
  if (e) { *cp = e->codepoint; return 1; }
  return 0;
}

/* Look up delimiter */
const char* mjx_lookup_delimiter(const char* name) {
  for (size_t i = 0; i < sizeof(DELIMITERS)/sizeof(DELIMITERS[0]); i++) {
    if (strcmp(DELIMITERS[i].name, name) == 0) return DELIMITERS[i].unicode;
  }
  /* Try single character */
  if (name && name[0] && !name[1]) {
    static char buf[8];
    buf[0] = name[0];
    buf[1] = '\0';
    return buf;
  }
  return NULL;
}

/* Build the operator dictionary */
mjx_operator* mjx_default_operators(size_t* count) {
  /* Collect all operator-like symbols */
  size_t total = 0;
  total += sizeof(GREEK_LOWER) / sizeof(GREEK_LOWER[0]);
  total += sizeof(GREEK_UPPER) / sizeof(GREEK_UPPER[0]);
  total += sizeof(BIN_OPS) / sizeof(BIN_OPS[0]);
  total += sizeof(RELATIONS) / sizeof(RELATIONS[0]);
  total += sizeof(LARGE_OPS) / sizeof(LARGE_OPS[0]);
  total += sizeof(MISC_SYMBOLS) / sizeof(MISC_SYMBOLS[0]);

  mjx_operator* ops = (mjx_operator*)calloc(total, sizeof(mjx_operator));
  if (!ops) { *count = 0; return NULL; }

  size_t idx = 0;
  const mjx_symbol_entry* tables[] = {
    GREEK_LOWER, GREEK_UPPER, BIN_OPS, RELATIONS, LARGE_OPS, MISC_SYMBOLS
  };
  size_t table_sizes[] = {
    sizeof(GREEK_LOWER)/sizeof(GREEK_LOWER[0]),
    sizeof(GREEK_UPPER)/sizeof(GREEK_UPPER[0]),
    sizeof(BIN_OPS)/sizeof(BIN_OPS[0]),
    sizeof(RELATIONS)/sizeof(RELATIONS[0]),
    sizeof(LARGE_OPS)/sizeof(LARGE_OPS[0]),
    sizeof(MISC_SYMBOLS)/sizeof(MISC_SYMBOLS[0]),
  };

  for (size_t t = 0; t < 6; t++) {
    for (size_t i = 0; i < table_sizes[t]; i++) {
      ops[idx].symbol = tables[t][i].cmd;
      ops[idx].texclass = tables[t][i].tex_class;
      ops[idx].form = 1; /* infix by default */
      ops[idx].lspace = -1;
      ops[idx].rspace = -1;
      ops[idx].largeop = tables[t][i].is_operator;
      idx++;
    }
  }

  *count = idx;
  return ops;
}
