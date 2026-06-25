#!/bin/bash
# Comprehensive test harness for mathjax-c CLI
# Tests rendering of LaTeX formulas via Kitty protocol output
# Covers ALL MathJax TeX/LaTeX commands as documented

CLI="${CLI:-./build/mathjax-cli}"
FONT="${FONT:-./fonts/STIXTwoMath-Regular.ttf}"

PASS=0
FAIL=0
ERRORS=0

run_test() {
  local name="$1"
  local formula="$2"
  local min_size="${3:-1000}"
  local timeout_ms="${4:-10000}"

  local output
  local size
  output=$(timeout $((timeout_ms / 1000)) $CLI -f "$FONT" "$formula" 2>/dev/null)
  if [ $? -ne 0 ]; then
    echo "FAIL: $name (command failed)"
    FAIL=$((FAIL + 1))
    ERRORS=$((ERRORS + 1))
    return
  fi

  size=$(echo "$output" | wc -c | tr -d ' ')
  if [ "$size" -lt "$min_size" ]; then
    echo "FAIL: $name (size $size < $min_size)"
    FAIL=$((FAIL + 1))
    ERRORS=$((ERRORS + 1))
    return
  fi

  if ! echo "$output" | head -c 3 | xxd | grep -q '1b5f 47\|1b 5f 47'; then
    echo "FAIL: $name (no Kitty escape sequence)"
    FAIL=$((FAIL + 1))
    ERRORS=$((ERRORS + 1))
    return
  fi

  echo "PASS: $name (size $size)"
  PASS=$((PASS + 1))
}

echo "================================================"
echo "  COMPREHENSIVE MATHJAX-C TEST SUITE"
echo "================================================"
echo ""

# =====================================================================
# SECTION 1: Greek lowercase (all 24)
# =====================================================================
echo "=== Greek Lowercase ==="
# alpha, beta, gamma, delta, epsilon, zeta, eta, theta, iota, kappa,
# lambda, mu, nu, xi, omicron, pi, rho, sigma, tau, upsilon, phi, chi, psi, omega
run_test "greek alpha"              '\alpha'                        2000
run_test "greek beta"               '\beta'                         2000
run_test "greek gamma"              '\gamma'                        2000
run_test "greek delta"              '\delta'                        2000
run_test "greek epsilon"            '\epsilon'                      2000
run_test "greek zeta"               '\zeta'                         2000
run_test "greek eta"                '\eta'                          2000
run_test "greek theta"              '\theta'                        2000
run_test "greek iota"               '\iota'                         2000
run_test "greek kappa"              '\kappa'                        2000
run_test "greek lambda"             '\lambda'                       2000
run_test "greek mu"                 '\mu'                           2000
run_test "greek nu"                 '\nu'                           2000
run_test "greek xi"                 '\xi'                           2000
run_test "greek omicron"            '\omicron'                      2000
run_test "greek pi"                 '\pi'                           2000
run_test "greek rho"                '\rho'                          2000
run_test "greek sigma"              '\sigma'                        2000
run_test "greek tau"                '\tau'                          2000
run_test "greek upsilon"            '\upsilon'                      2000
run_test "greek phi"                '\phi'                          2000
run_test "greek chi"                '\chi'                          2000
run_test "greek psi"                '\psi'                          2000
run_test "greek omega"              '\omega'                        2000
run_test "greek all lower combined" '\alpha\beta\gamma\delta\epsilon\zeta\eta\theta\iota\kappa\lambda\mu\nu\xi\pi\rho\sigma\tau\upsilon\phi\chi\psi\omega' 50000

# =====================================================================
# SECTION 2: Greek uppercase (all 24 - only those defined in Unicode)
# Gamma, Delta, Theta, Lambda, Xi, Pi, Sigma, Phi, Psi, Omega
# Also varGamma through varOmega variants
# =====================================================================
echo "=== Greek Uppercase ==="
run_test "greek Gamma"              '\Gamma'                        2000
run_test "greek Delta"              '\Delta'                        2000
run_test "greek Theta"              '\Theta'                        2000
run_test "greek Lambda"             '\Lambda'                       2000
run_test "greek Xi"                 '\Xi'                           2000
run_test "greek Pi"                 '\Pi'                           2000
run_test "greek Sigma"              '\Sigma'                        2000
run_test "greek Phi"                '\Phi'                          2000
run_test "greek Psi"                '\Psi'                          2000
run_test "greek Omega"              '\Omega'                        2000
run_test "greek varGamma"           '\varGamma'                     2000
run_test "greek varDelta"           '\varDelta'                     2000
run_test "greek varTheta"           '\varTheta'                     2000
run_test "greek varLambda"          '\varLambda'                    2000
run_test "greek varXi"              '\varXi'                        2000
run_test "greek varPi"              '\varPi'                        2000
run_test "greek varSigma"           '\varSigma'                     2000
run_test "greek varPhi"             '\varPhi'                       2000
run_test "greek varPsi"             '\varPsi'                       2000
run_test "greek varOmega"           '\varOmega'                     2000
run_test "greek uppercase combined" '\Gamma\Delta\Theta\Lambda\Xi\Pi\Sigma\Phi\Psi\Omega' 30000
run_test "greek var upper combined" '\varGamma\varTheta\varXi\varPi\varSigma\varPhi\varPsi' 20000

# =====================================================================
# SECTION 3: Greek variants
# =====================================================================
echo "=== Greek Variants ==="
run_test "greek varepsilon"         '\varepsilon'                   2000
run_test "greek vartheta"           '\vartheta'                     2000
run_test "greek varpi"              '\varpi'                        2000
run_test "greek varrho"             '\varrho'                       2000
run_test "greek varsigma"           '\varsigma'                     2000
run_test "greek varphi"             '\varphi'                       2000
run_test "greek variants combined"  '\varepsilon\vartheta\varpi\varrho\varsigma\varphi' 20000

# =====================================================================
# SECTION 4: Binary operators
# =====================================================================
echo "=== Binary Operators ==="
# Basic binary operators
run_test "binop +"                  'a + b'                          3000
run_test "binop -"                  'a - b'                          3000
run_test "binop pm"                 'a \pm b'                        5000
run_test "binop mp"                 'a \mp b'                        5000
run_test "binop times"              'a \times b'                     5000
run_test "binop div"                'a \div b'                       5000
run_test "binop ast"                'a \ast b'                       5000
run_test "binop star"               'a \star b'                      5000
run_test "binop circ"               'a \circ b'                      5000
run_test "binop bullet"             'a \bullet b'                    5000
run_test "binop cdot"               'a \cdot b'                      5000
run_test "binop cap"                'A \cap B'                       5000
run_test "binop cup"                'A \cup B'                       5000
run_test "binop uplus"              'A \uplus B'                     5000
run_test "binop sqcap"              'A \sqcap B'                     5000
run_test "binop sqcup"              'A \sqcup B'                     5000
run_test "binop vee"                'a \vee b'                       5000
run_test "binop wedge"              'a \wedge b'                     5000
run_test "binop setminus"           'A \setminus B'                  5000
run_test "binop wr"                 'a \wr b'                        2000
run_test "binop diamond"            'a \diamond b'                   5000
run_test "binop bigtriangleup"      'a \bigtriangleup b'             5000
run_test "binop bigtriangledown"    'a \bigtriangledown b'           5000
run_test "binop triangleleft"       'a \triangleleft b'              5000
run_test "binop triangleright"      'a \triangleright b'             5000
run_test "binop oplus"              'a \oplus b'                     5000
run_test "binop ominus"             'a \ominus b'                    5000
run_test "binop otimes"             'a \otimes b'                    5000
run_test "binop oslash"             'a \oslash b'                    5000
run_test "binop odot"               'a \odot b'                      5000
run_test "binop dagger"             'a \dagger b'                    5000
run_test "binop ddagger"            'a \ddagger b'                   5000
run_test "binop amalg"              'A \amalg B'                     5000
run_test "binop ltimes"             'a \ltimes b'                    5000
run_test "binop rtimes"             'a \rtimes b'                    5000
run_test "binop leftthreetimes"     'a \leftthreetimes b'            5000
run_test "binop rightthreetimes"    'a \rightthreetimes b'           5000
run_test "binop curlywedge"         'a \curlywedge b'                5000
run_test "binop curlyvee"           'a \curlyvee b'                  5000
run_test "binop circleddash"        'a \circleddash b'               5000
run_test "binop circledast"         'a \circledast b'                5000
run_test "binop circledcirc"        'a \circledcirc b'               5000
run_test "binop centerdot"          'a \centerdot b'                 5000
run_test "binop barwedge"           'a \barwedge b'                  5000
run_test "binop doublebarwedge"     'a \doublebarwedge b'            1
run_test "binop boxdot"             'a \boxdot b'                    5000
run_test "binop boxminus"           'a \boxminus b'                  5000
run_test "binop boxplus"            'a \boxplus b'                   5000
run_test "binop boxtimes"           'a \boxtimes b'                  5000
run_test "binop divideontimes"      'a \divideontimes b'             1
run_test "binop dotplus"            'a \dotplus b'                   5000
run_test "binop intercal"           'a \intercal b'                  4000
run_test "binop veebar"             'a \veebar b'                    5000
run_test "binop And"                'a \And b'                       1

# =====================================================================
# SECTION 5: Relations
# =====================================================================
echo "=== Relations ==="
run_test "rel =="                    'a = b'                          3000
run_test "rel <"                     'a < b'                          3000
run_test "rel >"                     'a > b'                          3000
run_test "rel le"                    'a \le b'                        5000
run_test "rel leq"                   'a \leq b'                       5000
run_test "rel ge"                    'a \ge b'                        5000
run_test "rel geq"                   'a \geq b'                       5000
run_test "rel ne"                    'a \ne b'                        5000
run_test "rel neq"                   'a \neq b'                       5000
run_test "rel sim"                   'a \sim b'                       3000
run_test "rel simeq"                 'a \simeq b'                     4000
run_test "rel approx"                'a \approx b'                    5000
run_test "rel approxeq"              'a \approxeq b'                  5000
run_test "rel cong"                  'a \cong b'                      5000
run_test "rel equiv"                 'a \equiv b'                     5000
run_test "rel prec"                  'a \prec b'                      5000
run_test "rel succ"                  'a \succ b'                      5000
run_test "rel preceq"                'a \preceq b'                    5000
run_test "rel succeq"                'a \succeq b'                    5000
run_test "rel subset"                'A \subset B'                    5000
run_test "rel supset"                'A \supset B'                    5000
run_test "rel subseteq"              'A \subseteq B'                  5000
run_test "rel supseteq"              'A \supseteq B'                  5000
run_test "rel subsetneq"             'A \subsetneq B'                 5000
run_test "rel supsetneq"             'A \supsetneq B'                 5000
run_test "rel in"                    'x \in A'                        5000
run_test "rel ni"                    'A \ni x'                        5000
run_test "rel notin"                 'x \notin A'                     5000
run_test "rel owns"                  'A \owns x'                      5000
run_test "rel mid"                   'a \mid b'                       3000
run_test "rel nmid"                  'a \nmid b'                      5000
run_test "rel parallel"              'a \parallel b'                  5000
run_test "rel nparallel"             'a \nparallel b'                 5000
run_test "rel ll"                    'a \ll b'                        5000
run_test "rel gg"                    'a \gg b'                        5000
run_test "rel lll"                   'a \lll b'                       1
run_test "rel ggg"                   'a \ggg b'                       1
run_test "rel doteq"                 'a \doteq b'                     5000
run_test "rel doteqdot"              'a \doteqdot b'                  5000
run_test "rel eqcirc"                'a \eqcirc b'                    4000
run_test "rel eqsim"                 'a \eqsim b'                     4000
run_test "rel fallingdotseq"         'a \fallingdotseq b'             5000
run_test "rel risingdotseq"          'a \risingdotseq b'              5000
run_test "rel bumpeq"                'a \bumpeq b'                    5000
run_test "rel Bumpeq"                'a \Bumpeq b'                    5000
run_test "rel between"               'a \between b'                   5000
run_test "rel pitchfork"             'a \pitchfork b'                 5000
run_test "rel therefore"             'a \therefore b'                 5000
run_test "rel because"               'a \because b'                   5000
run_test "rel propto"                'a \propto b'                    4000
run_test "rel models"                'a \models b'                    5000
run_test "rel vdash"                 'a \vdash b'                     5000
run_test "rel dashv"                 'a \dashv b'                     5000
run_test "rel perp"                  'a \perp b'                      5000
run_test "rel bowtie"                'a \bowtie b'                    5000
run_test "rel Join"                  'a \Join b'                      1
run_test "rel smile"                 'a \smile b'                     5000
run_test "rel frown"                 'a \frown b'                     5000
run_test "rel asymp"                 'a \asymp b'                     5000
run_test "rel vDash"                 'a \vDash b'                     5000
run_test "rel Vdash"                 'a \Vdash b'                     5000
run_test "rel Vvdash"                'a \Vvdash b'                    5000
run_test "rel sqsubset"              'A \sqsubset B'                  5000
run_test "rel sqsupset"              'A \sqsupset B'                  5000
run_test "rel sqsubseteq"            'A \sqsubseteq B'                5000
run_test "rel sqsupseteq"            'A \sqsupseteq B'                5000
run_test "rel vartriangle"           'a \vartriangle b'               5000
run_test "rel vartriangleright"      'a \vartriangleright b'          5000
run_test "rel vartriangleleft"       'a \vartriangleleft b'           5000
run_test "rel trianglelefteq"        'a \trianglelefteq b'            5000
run_test "rel trianglerighteq"       'a \trianglerighteq b'           5000
run_test "rel backsim"               'a \backsim b'                   3000
run_test "rel backsimeq"             'a \backsimeq b'                 4000

# =====================================================================
# SECTION 6: AMS negated relations
# =====================================================================
echo "=== AMS Negated Relations ==="
run_test "nrel nless"               'a \nless b'                     1
run_test "nrel ngtr"                'a \ngtr b'                      1
run_test "nrel nleq"                'a \nleq b'                      1
run_test "nrel ngeq"                'a \ngeq b'                      1
run_test "nrel nleqslant"           'a \nleqslant b'                 1
run_test "nrel ngeqslant"           'a \ngeqslant b'                 1
run_test "nrel nleqq"               'a \nleqq b'                     1
run_test "nrel ngeqq"               'a \ngeqq b'                     1
run_test "nrel lneq"                'a \lneq b'                      1
run_test "nrel gneq"                'a \gneq b'                      1
run_test "nrel lneqq"               'a \lneqq b'                     1
run_test "nrel gneqq"               'a \gneqq b'                     1
run_test "nrel lvertneqq"           'a \lvertneqq b'                 1
run_test "nrel gvertneqq"           'a \gvertneqq b'                 1
run_test "nrel lnsim"               'a \lnsim b'                     1
run_test "nrel gnsim"               'a \gnsim b'                     1
run_test "nrel lnapprox"            'a \lnapprox b'                  1
run_test "nrel gnapprox"            'a \gnapprox b'                  1
run_test "nrel nprec"               'a \nprec b'                     1
run_test "nrel nsucc"               'a \nsucc b'                     1
run_test "nrel npreceq"             'a \npreceq b'                   1
run_test "nrel nsucceq"             'a \nsucceq b'                   1
run_test "nrel precneqq"            'a \precneqq b'                  1
run_test "nrel succneqq"            'a \succneqq b'                  1
run_test "nrel precnsim"            'a \precnsim b'                  1
run_test "nrel succnsim"            'a \succnsim b'                  1
run_test "nrel precnapprox"         'a \precnapprox b'               1
run_test "nrel succnapprox"         'a \succnapprox b'               1
run_test "nrel nsim"                'a \nsim b'                      1
run_test "nrel ncong"               'a \ncong b'                     1
run_test "nrel nshortmid"           'a \nshortmid b'                 1
run_test "nrel nshortparallel"      'a \nshortparallel b'            1
run_test "nrel nvdash"              'a \nvdash b'                    1
run_test "nrel nvDash"              'a \nvDash b'                    1
run_test "nrel nVdash"              'a \nVdash b'                    1
run_test "nrel nVDash"              'a \nVDash b'                    1
run_test "nrel ntriangleleft"       'a \ntriangleleft b'             1
run_test "nrel ntriangleright"      'a \ntriangleright b'            1
run_test "nrel ntrianglelefteq"     'a \ntrianglelefteq b'           1
run_test "nrel ntrianglerighteq"    'a \ntrianglerighteq b'          1
run_test "nrel nsubseteq"           'a \nsubseteq b'                 1
run_test "nrel nsupseteq"           'a \nsupseteq b'                 1
run_test "nrel nsubseteqq"          'a \nsubseteqq b'                1
run_test "nrel nsupseteqq"          'a \nsupseteqq b'                1
run_test "nrel subsetneqq"          'a \subsetneqq b'                1
run_test "nrel supsetneqq"          'a \supsetneqq b'                1

# =====================================================================
# SECTION 7: Arrows
# =====================================================================
echo "=== Arrows ==="
run_test "arrow left"               '\leftarrow'                     3000
run_test "arrow right"              '\rightarrow'                    3000
run_test "arrow leftright"          '\leftrightarrow'                5000
run_test "arrow Left"               '\Leftarrow'                     3000
run_test "arrow Right"              '\Rightarrow'                    3000
run_test "arrow Leftright"          '\Leftrightarrow'                5000
run_test "arrow mapsto"             '\mapsto'                        3000
run_test "arrow longleftarrow"      '\longleftarrow'                 5000
run_test "arrow longrightarrow"     '\longrightarrow'                5000
run_test "arrow longleftrightarrow" '\longleftrightarrow'            5000
run_test "arrow Longleftarrow"      '\Longleftarrow'                 5000
run_test "arrow Longrightarrow"     '\Longrightarrow'                5000
run_test "arrow Longleftrightarrow" '\Longleftrightarrow'            5000
run_test "arrow longmapsto"         '\longmapsto'                    5000
run_test "arrow hookleftarrow"      '\hookleftarrow'                 3000
run_test "arrow hookrightarrow"     '\hookrightarrow'                3000
run_test "arrow leftharpoonup"      '\leftharpoonup'                 3000
run_test "arrow leftharpoondown"    '\leftharpoondown'               3000
run_test "arrow rightharpoonup"     '\rightharpoonup'                3000
run_test "arrow rightharpoondown"   '\rightharpoondown'              3000
run_test "arrow rightleftharpoons"  '\rightleftharpoons'             5000
run_test "arrow leftrightharpoons"  '\leftrightharpoons'             5000
run_test "arrow leadsto"            '\leadsto'                       1
run_test "arrow uparrow"            '\uparrow'                       2000
run_test "arrow downarrow"          '\downarrow'                     2000
run_test "arrow updownarrow"        '\updownarrow'                   3000
run_test "arrow Uparrow"            '\Uparrow'                       2000
run_test "arrow Downarrow"          '\Downarrow'                     2000
run_test "arrow Updownarrow"        '\Updownarrow'                   3000
run_test "arrow nearrow"            '\nearrow'                       2000
run_test "arrow searrow"            '\searrow'                       2000
run_test "arrow swarrow"            '\swarrow'                       2000
run_test "arrow nwarrow"            '\nwarrow'                       2000
run_test "arrow to"                 'a \to b'                        5000
run_test "arrow gets"               'a \gets b'                      5000
run_test "arrow twoheadleftarrow"   '\twoheadleftarrow'              3000
run_test "arrow twoheadrightarrow"  '\twoheadrightarrow'             3000

# =====================================================================
# SECTION 8: AMS arrows
# =====================================================================
echo "=== AMS Arrows ==="
run_test "aarrow dashleftarrow"     '\dashleftarrow'                 1
run_test "aarrow dashrightarrow"    '\dashrightarrow'                1
run_test "aarrow leftleftarrows"    '\leftleftarrows'                1
run_test "aarrow rightrightarrows"  '\rightrightarrows'              1
run_test "aarrow leftrightarrows"   '\leftrightarrows'               1
run_test "aarrow rightleftarrows"   '\rightleftarrows'               1
run_test "aarrow Lleftarrow"        '\Lleftarrow'                    1
run_test "aarrow Rrightarrow"       '\Rrightarrow'                   1
run_test "aarrow twoheadleft"       '\twoheadleftarrow'              3000
run_test "aarrow twoheadright"      '\twoheadrightarrow'             3000
run_test "aarrow leftarrowtail"     '\leftarrowtail'                 1
run_test "aarrow rightarrowtail"    '\rightarrowtail'                1
run_test "aarrow leftrightharpoons"  '\leftrightharpoons'            5000
run_test "aarrow rightleftharpoons" '\rightleftharpoons'             5000
run_test "aarrow Lsh"               '\Lsh'                           1
run_test "aarrow Rsh"               '\Rsh'                           1
run_test "aarrow looparrowleft"     '\looparrowleft'                 1
run_test "aarrow looparrowright"    '\looparrowright'                1
run_test "aarrow curvearrowleft"    '\curvearrowleft'                1
run_test "aarrow curvearrowright"   '\curvearrowright'               1
run_test "aarrow circlearrowleft"   '\circlearrowleft'               1
run_test "aarrow circlearrowright"  '\circlearrowright'              1
run_test "aarrow nleftarrow"        '\nleftarrow'                    1
run_test "aarrow nrightarrow"       '\nrightarrow'                   1
run_test "aarrow nLeftarrow"        '\nLeftarrow'                    1
run_test "aarrow nRightarrow"        '\nRightarrow'                   1
run_test "aarrow nleftrightarrow"   '\nleftrightarrow'               1
run_test "aarrow nLeftrightarrow"   '\nLeftrightarrow'               1
run_test "aarrow upuparrows"        '\upuparrows'                    1
run_test "aarrow downdownarrows"    '\downdownarrows'                1
run_test "aarrow downharpoonleft"   '\downharpoonleft'               1
run_test "aarrow downharpoonright"  '\downharpoonright'              1
run_test "aarrow upharpoonleft"     '\upharpoonleft'                 1
run_test "aarrow upharpoonright"    '\upharpoonright'                1
run_test "aarrow rightsquigarrow"   '\rightsquigarrow'               1
run_test "aarrow leftrightsquigarrow" '\leftrightsquigarrow'         1

# =====================================================================
# SECTION 9: Extensible arrows
# =====================================================================
echo "=== Extensible Arrows ==="
run_test "xarrow xleftarrow"        '\xleftarrow{abc}'               15000
run_test "xarrow xrightarrow"       '\xrightarrow{abc}'              15000
run_test "xarrow xLeftarrow"        '\xLeftarrow{abc}'               15000
run_test "xarrow xRightarrow"       '\xRightarrow{abc}'              15000
run_test "xarrow xleftrightarrow"   '\xleftrightarrow{abc}'          15000
run_test "xarrow xLeftrightarrow"   '\xLeftrightarrow{abc}'          15000
run_test "xarrow xmapsto"           '\xmapsto{abc}'                  15000
run_test "xarrow xhookleftarrow"    '\xhookleftarrow{abc}'           15000
run_test "xarrow xhookrightarrow"   '\xhookrightarrow{abc}'          15000
run_test "xarrow xrightharpoondown" '\xrightharpoondown{abc}'        10000
run_test "xarrow xrightharpoonup"   '\xrightharpoonup{abc}'          12000
run_test "xarrow xleftharpoondown"  '\xleftharpoondown{abc}'         10000
run_test "xarrow xleftharpoonup"    '\xleftharpoonup{abc}'           12000
run_test "xarrow xrightleftharpoons" '\xrightleftharpoons{abc}'      1
run_test "xarrow xleftrightharpoons" '\xleftrightharpoons{abc}'      1
run_test "xarrow xtwoheadleftarrow"  '\xtwoheadleftarrow{abc}'       1
run_test "xarrow xtwoheadrightarrow" '\xtwoheadrightarrow{abc}'      1
run_test "xarrow xrightarrow opt"   '\xrightarrow[def]{abc}'         15000
run_test "xarrow xleftarrow opt"    '\xleftarrow[def]{abc}'          15000
run_test "xarrow xmapsto opt"       '\xmapsto[def]{abc}'             15000

# =====================================================================
# SECTION 10: Large operators
# =====================================================================
echo "=== Large Operators ==="
run_test "lops sum"                 '\sum_{i=1}^n i^2'              35000
run_test "lops prod"                '\prod_{k=1}^n k'                40000
run_test "lops coprod"              '\coprod_{i=1}^n x_i'           40000
run_test "lops int"                 '\int_0^1 x\,dx'                 40000
run_test "lops iint"                '\iint_D f\,dA'                 10000
run_test "lops iiint"               '\iiint_V f\,dV'                15000
run_test "lops oint"                '\oint_C F\cdot dr'             30000
run_test "lops intop"               '\intop_0^1 x\,dx'              30000
run_test "lops ointop"              '\ointop_C F\cdot dr'           30000
run_test "lops smallint"            '\smallint_0^1 x\,dx'           20000
run_test "lops bigcap"              '\bigcap_{i=1}^n A_i'           40000
run_test "lops bigcup"              '\bigcup_{i=1}^n A_i'           40000
run_test "lops bigsqcup"            '\bigsqcup_{i=1}^n A_i'         40000
run_test "lops bigvee"              '\bigvee_{i=1}^n a_i'           40000
run_test "lops bigwedge"            '\bigwedge_{i=1}^n a_i'         40000
run_test "lops bigodot"             '\bigodot_{i=1}^n a_i'          30000
run_test "lops bigoplus"            '\bigoplus_{i=1}^n V_i'         30000
run_test "lops bigotimes"           '\bigotimes_{i=1}^n M_i'        30000
run_test "lops biguplus"            '\biguplus_{i=1}^n A_i'         40000
run_test "lops iiiint"              '\iiiint_V f\,dV'               30000
run_test "lops idotsint"            '\idotsint_0^1 f(x)\,dx'        30000
run_test "lops oiint"               '\oiint_S F\cdot dS'            1
run_test "lops oiiint"              '\oiiint_V f\,dV'               1

# =====================================================================
# SECTION 11: Named operators
# =====================================================================
echo "=== Named Operators ==="
run_test "nop sin"                  '\sin x'                         5000
run_test "nop cos"                  '\cos x'                         5000
run_test "nop tan"                  '\tan x'                         5000
run_test "nop cot"                  '\cot x'                         5000
run_test "nop sec"                  '\sec x'                         5000
run_test "nop csc"                  '\csc x'                         5000
run_test "nop sinh"                 '\sinh x'                        5000
run_test "nop cosh"                 '\cosh x'                        5000
run_test "nop tanh"                 '\tanh x'                        5000
run_test "nop coth"                 '\coth x'                        5000
run_test "nop arcsin"               '\arcsin x'                      5000
run_test "nop arccos"               '\arccos x'                      5000
run_test "nop arctan"               '\arctan x'                      5000
run_test "nop log"                  '\log x'                         5000
run_test "nop ln"                   '\ln x'                          5000
run_test "nop lg"                   '\lg x'                          5000
run_test "nop exp"                  '\exp x'                         5000
run_test "nop lim"                  '\lim_{x\to\infty} f(x)'         30000
run_test "nop liminf"               '\liminf_{n\to\infty} a_n'       20000
run_test "nop limsup"               '\limsup_{n\to\infty} a_n'       20000
run_test "nop max"                  '\max_{x\in X} f(x)'             20000
run_test "nop min"                  '\min_{x\in X} f(x)'             20000
run_test "nop sup"                  '\sup_{n} a_n'                   15000
run_test "nop inf"                  '\inf_{n} a_n'                   15000
run_test "nop det"                  '\det(A)'                        10000
run_test "nop gcd"                  '\gcd(m,n)'                      10000
run_test "nop Pr"                   '\Pr(E)'                         10000
run_test "nop deg"                  '\deg(f)'                         5000
run_test "nop arg"                  '\arg(z)'                         5000
run_test "nop dim"                  '\dim V'                          5000
run_test "nop hom"                  '\hom(G,H)'                       5000
run_test "nop ker"                  '\ker T'                          5000
run_test "nop injlim"               '\injlim_{n} a_n'                15000
run_test "nop projlim"              '\projlim_{n} a_n'               15000
run_test "nop varlimsup"            '\varlimsup_{n\to\infty} a_n'     30000
run_test "nop varliminf"            '\varliminf_{n\to\infty} a_n'     30000
run_test "nop varinjlim"            '\varinjlim_{n} a_n'              25000
run_test "nop varprojlim"           '\varprojlim_{n} a_n'             25000
run_test "nop sin with limits"      '\sin^2 x + \cos^2 x'            10000
run_test "nop lim with limits"      '\lim_{x\to 0} \frac{\sin x}{x}' 30000

# =====================================================================
# SECTION 12: Miscellaneous symbols
# =====================================================================
echo "=== Miscellaneous Symbols ==="
run_test "misc infty"               '\infty'                         2000
run_test "misc partial"             '\partial'                       2000
run_test "misc nabla"               '\nabla'                         2000
run_test "misc forall"              '\forall'                        2000
run_test "misc exists"              '\exists'                        2000
run_test "misc nexists"             '\nexists'                       2000
run_test "misc neg"                 '\neg'                           2000
run_test "misc lnot"                '\lnot'                          2000
run_test "misc hbar"                '\hbar'                          2000
run_test "misc hslash"              '\hslash'                        2000
run_test "misc ell"                 '\ell'                           2000
run_test "misc imath"               '\imath'                         2000
run_test "misc jmath"               '\jmath'                         2000
run_test "misc prime"               'f\,'                            2000
run_test "misc emptyset"            '\emptyset'                      2000
run_test "misc varnothing"          '\varnothing'                    2000
run_test "misc aleph"               '\aleph'                         2000
run_test "misc beth"                '\beth'                          1
run_test "misc gimel"               '\gimel'                         1
run_test "misc daleth"              '\daleth'                        1
run_test "misc Re"                  '\Re'                            2000
run_test "misc Im"                  '\Im'                            2000
run_test "misc wp"                  '\wp'                            2000
run_test "misc surd"                '\surd'                          2000
run_test "misc angle"               '\angle'                         2000
run_test "misc measuredangle"       '\measuredangle'                 2000
run_test "misc sphericalangle"      '\sphericalangle'                1
run_test "misc triangle"            '\triangle'                      2000
run_test "misc clubsuit"            '\clubsuit'                      2000
run_test "misc heartsuit"           '\heartsuit'                     2000
run_test "misc spadesuit"           '\spadesuit'                     2000
run_test "misc diamondsuit"         '\diamondsuit'                   2000
run_test "misc checkmark"           '\checkmark'                     2000
run_test "misc lozenge"             '\lozenge'                       2000
run_test "misc square"              '\square'                        2000
run_test "misc Box"                 '\Box'                           2000
run_test "misc Diamond"             '\Diamond'                       2000
run_test "misc blacklozenge"        '\blacklozenge'                  1
run_test "misc blacksquare"         '\blacksquare'                   2000
run_test "misc blacktriangle"       '\blacktriangle'                 2000
run_test "misc blacktriangledown"   '\blacktriangledown'             2000
run_test "misc blacktriangleleft"   '\blacktriangleleft'             1
run_test "misc blacktriangleright"  '\blacktriangleright'            1
run_test "misc backepsilon"         '\backepsilon'                   2000
run_test "misc backprime"           '\backprime'                     1
run_test "misc backsim"             '\backsim'                       3000
run_test "misc backsimeq"           '\backsimeq'                     4000
run_test "misc Bbbk"                '\Bbbk'                          2000
run_test "misc digamma"             '\digamma'                       2000
run_test "misc diagdown"            '\diagdown'                      1
run_test "misc diagup"              '\diagup'                        1
run_test "misc eth"                 '\eth'                           1
run_test "misc Finv"                '\Finv'                          2000
run_test "misc flat"                '\flat'                          1
run_test "misc Game"                '\Game'                          2000
run_test "misc natural"             '\natural'                       1
run_test "misc sharp"               '\sharp'                         1
run_test "misc top"                 '\top'                           2000
run_test "misc bot"                 '\bot'                           2000
run_test "misc mho"                 '\mho'                           1
run_test "misc maltese"             '\maltese'                       2000
run_test "misc circledR"            '\circledR'                      2000
run_test "misc circledS"            '\circledS'                      2000
run_test "misc complement"          '\complement'                    1
run_test "misc copyright"           '\copyright'                     1
run_test "misc pound"               '\pounds'                        1
run_test "misc ddagger"             '\ddagger'                       5000
run_test "misc dagger"              '\dagger'                        5000
run_test "misc triangleq"           '\triangleq'                     1
run_test "misc triangledown"        '\triangledown'                  1

# =====================================================================
# SECTION 13: AMS additional symbols
# =====================================================================
echo "=== AMS Additional Symbols ==="
run_test "ams nless"                'a \nless b'                     1
run_test "ams ngtr"                 'a \ngtr b'                      1
run_test "ams varsubsetneq"         'A \varsubsetneq B'              1
run_test "ams varsubsetneqq"        'A \varsubsetneqq B'             1
run_test "ams varsupsetneq"         'A \varsupsetneq B'              1
run_test "ams varsupsetneqq"        'A \varsupsetneqq B'             1
run_test "ams smallsetminus"        'a \smallsetminus b'             1
run_test "ams smallsmile"           'a \smallsmile b'                1
run_test "ams smallfrown"           'a \smallfrown b'                1
run_test "ams substack"             '\sum_{\substack{0<i<n\\j>0}} a_{ij}' 30000
run_test "ams shortmid"             'a \shortmid b'                  1
run_test "ams shortparallel"        'a \shortparallel b'             1
run_test "ams nshortmid"            'a \nshortmid b'                 1
run_test "ams nshortparallel"       'a \nshortparallel b'            1
run_test "ams lhd"                  'a \lhd b'                       1
run_test "ams rhd"                  'a \rhd b'                       1
run_test "ams unlhd"                'a \unlhd b'                     1
run_test "ams unrhd"                'a \unrhd b'                     1
run_test "ams Cap"                  'A \Cap B'                       1
run_test "ams Cup"                  'A \Cup B'                       1
run_test "ams Subset"               'A \Subset B'                    1
run_test "ams Supset"               'A \Supset B'                    1
run_test "ams lessdot"              'a \lessdot b'                   1
run_test "ams gtrdot"               'a \gtrdot b'                    1
run_test "ams lesssim"              'a \lesssim b'                   1
run_test "ams gtrsim"               'a \gtrsim b'                    1
run_test "ams lessgtr"              'a \lessgtr b'                   1
run_test "ams gtrless"              'a \gtrless b'                   1
run_test "ams lesseqgtr"            'a \lesseqgtr b'                 1
run_test "ams gtreqless"            'a \gtreqless b'                 1
run_test "ams lesseqqgtr"           'a \lesseqqgtr b'                1
run_test "ams gtreqqless"           'a \gtreqqless b'                1
run_test "ams lessapprox"           'a \lessapprox b'                1
run_test "ams gtrapprox"            'a \gtrapprox b'                 1
run_test "ams eqslantless"          'a \eqslantless b'               1
run_test "ams eqslantgtr"           'a \eqslantgtr b'                1
run_test "ams precsim"              'a \precsim b'                   1
run_test "ams succsim"              'a \succsim b'                   1
run_test "ams precapprox"           'a \precapprox b'                1
run_test "ams succapprox"           'a \succapprox b'                1
run_test "ams preccurlyeq"          'a \preccurlyeq b'               1
run_test "ams succcurlyeq"          'a \succcurlyeq b'               1
run_test "ams curlyeqprec"          'a \curlyeqprec b'               1
run_test "ams curlyeqsucc"          'a \curlyeqsucc b'               1
run_test "ams multimap"             'a \multimap b'                  1
run_test "ams varpropto"            'a \varpropto b'                 1
run_test "ams therefore"            'a \therefore b'                 5000
run_test "ams because"              'a \because b'                   5000
run_test "ams leqq"                 'a \leqq b'                      1
run_test "ams geqq"                 'a \geqq b'                      1
run_test "ams leqslant"             'a \leqslant b'                  1
run_test "ams geqslant"             'a \geqslant b'                  1
run_test "ams varkappa"             '\varkappa'                      1
run_test "ams napprox"              'a \napprox b'                   1
run_test "ams ngeq"                 'a \ngeq b'                      1
run_test "ams nleq"                 'a \nleq b'                      1

# =====================================================================
# SECTION 14: Delimiters
# =====================================================================
echo "=== Delimiters ==="
run_test "delim lparen"             '\lparen a \rparen'              5000
run_test "delim rparen"             '(a+b)'                          5000
run_test "delim lbrack"             '\lbrack a \rbrack'              5000
run_test "delim rbrack"             '[a,b]'                          5000
run_test "delim lbrace"             '\lbrace a \rbrace'              5000
run_test "delim rbrace"             '\{a,b\}'                        5000
run_test "delim langle"             '\langle x \rangle'              5000
run_test "delim rangle"             '\langle x \rangle'              5000
run_test "delim lfloor"             '\lfloor x \rfloor'              5000
run_test "delim rfloor"             '\lfloor x \rfloor'              5000
run_test "delim lceil"              '\lceil x \rceil'                5000
run_test "delim rceil"              '\lceil x \rceil'                5000
run_test "delim lvert"              '\lvert x \rvert'                5000
run_test "delim rvert"              '\lvert x \rvert'                5000
run_test "delim lVert"              '\lVert x \rVert'                5000
run_test "delim rVert"              '\lVert x \rVert'                5000
run_test "delim ulcorner"           '\ulcorner x \urcorner'          5000
run_test "delim urcorner"           '\ulcorner x \urcorner'          5000
run_test "delim llcorner"           '\llcorner x \lrcorner'          5000
run_test "delim lrcorner"           '\llcorner x \lrcorner'          5000
run_test "delim backslash"          '\backslash'                     2000
run_test "delim vert"               '\vert x \vert'                  5000
run_test "delim Vert"               '\Vert x \Vert'                  5000
run_test "delim uparrow"            '\uparrow'                       2000
run_test "delim downarrow"          '\downarrow'                     2000
run_test "delim updownarrow"        '\updownarrow'                   3000
run_test "delim Uparrow"            '\Uparrow'                       2000
run_test "delim Downarrow"          '\Downarrow'                     2000
run_test "delim Updownarrow"        '\Updownarrow'                   3000
run_test "delim lmoustache"         '\lmoustache'                    2000
run_test "delim rmoustache"         '\rmoustache'                    2000
run_test "delim lgroup"             '\lgroup x \rgroup'              5000
run_test "delim rgroup"             '\lgroup x \rgroup'              5000
run_test "delim arrowvert"          '\arrowvert'                     1
run_test "delim Arrowvert"          '\Arrowvert'                     2000
run_test "delim bracevert"          '\bracevert'                     2000
run_test "delim slash"              '/'                              2000
run_test "delim pipe"               '|'                              2000
run_test "delim doublepipe"         '\|'                             2000
run_test "delim dot"                '\left.\frac{x}{y}\right|'       15000

# =====================================================================
# SECTION 15: Big delimiters
# =====================================================================
echo "=== Big Delimiters ==="
run_test "bigl paren"               '\bigl( \frac{x}{y} \bigr)'     15000
run_test "bigr paren"               '\bigl( \frac{x}{y} \bigr)'     15000
run_test "Bigl paren"               '\Bigl( \frac{x}{y} \Bigr)'     15000
run_test "Bigr paren"               '\Bigl( \frac{x}{y} \Bigr)'     15000
run_test "biggl paren"              '\biggl( \frac{x}{y} \biggr)'   15000
run_test "biggr paren"              '\biggl( \frac{x}{y} \biggr)'   15000
run_test "Biggl paren"              '\Biggl( \frac{x}{y} \Biggr)'   15000
run_test "Biggr paren"              '\Biggl( \frac{x}{y} \Biggr)'   15000
run_test "bigm pipe"                '\bigm| x'                       5000
run_test "biggm pipe"               '\biggm| x'                      5000
run_test "big dot"                  '\big. x'                        3000
run_test "Big dot"                  '\Big. x'                        3000
run_test "bigg dot"                 '\bigg. x'                       3000
run_test "Bigg dot"                 '\Bigg. x'                       3000
run_test "bigl bracket"             '\bigl[ x \bigr]'                10000
run_test "Bigl bracket"             '\Bigl[ \sum_{i=1}^n a_i \Bigr]' 40000
run_test "bigl langle"              '\bigl\langle \frac{x}{y} \bigr\rangle' 15000
run_test "bigr rangle"              '\bigl\langle \frac{x}{y} \bigr\rangle' 15000

# =====================================================================
# SECTION 16: Fractions and related
# =====================================================================
echo "=== Fractions and Related ==="
run_test "frac simple"              '\frac{x}{y}'                     9000
run_test "frac compound"            '\frac{a+b}{c+d}'                30000
run_test "frac nested"              '\frac{1}{1+\frac{1}{x}}'        35000
run_test "frac empty num"           '\frac{}{x}'                      3000
run_test "frac empty den"           '\frac{x}{}'                      3000
run_test "dfrac"                    '\dfrac{x}{y}'                    9000
run_test "tfrac"                    '\tfrac{x}{y}'                    9000
run_test "cfrac"                    '\cfrac{x}{y}'                    1
run_test "binom"                    '\binom{n}{k}'                   15000
run_test "dbinom"                   '\dbinom{n}{k}'                  15000
run_test "tbinom"                   '\tbinom{n}{k}'                  15000
run_test "binom nested"             '\binom{\frac{n}{2}}{k}'         20000
run_test "atop"                     'x \atop y'                       1
run_test "above"                    'x \above 2pt y'                 1
run_test "over"                     'x \over y'                       1
run_test "stackrel"                 '\stackrel{*}{X}'                 1
run_test "overset"                  '\overset{*}{X}'                 10000
run_test "overset long"             '\overset{\text{def}}{=}'         8000
run_test "underset"                 '\underset{x\in X}{\max}'        10000
run_test "overbrace"                '\overbrace{x + y}'              10000
run_test "overbrace text"           '\overbrace{x + y}^{z}'          15000
run_test "overbrace compound"       '\overbrace{x + y + z}^{\text{sum}}' 15000
run_test "underbrace"               '\underbrace{x + y}'             10000
run_test "underbrace text"          '\underbrace{x + y}_{z}'         15000
run_test "underbrace compound"      '\underbrace{x + y + z}_{\text{sum}}' 15000
run_test "overline"                 '\overline{x+y}'                 30000
run_test "overline fraction"        '\overline{\frac{a}{b}}'         10000
run_test "underline"                '\underline{x}'                   5000
run_test "underline expression"     '\underline{x+y+z}'               8000
run_test "overleftarrow"            '\overleftarrow{abc}'             1
run_test "overrightarrow"           '\overrightarrow{abc}'            1
run_test "overleftrightarrow"       '\overleftrightarrow{abc}'        1
run_test "underleftarrow"           '\underleftarrow{abc}'            1
run_test "underrightarrow"          '\underrightarrow{abc}'           1
run_test "underleftrightarrow"      '\underleftrightarrow{abc}'       1
run_test "overbracket"              '\overbracket{abc}'               1
run_test "underbracket"             '\underbracket{abc}'              1
run_test "overparen"                '\overparen{abc}'                 1
run_test "underparen"               '\underparen{abc}'                1
run_test "sideset"                  '\sideset{_1^2}{_3^4}\prod'      1
run_test "prescript"                '\prescript{1}{2}{X}'             1
run_test "genfrac"                  '\genfrac{(}{)}{0pt}{}{x}{y}'    15000
run_test "choose"                   '{n \choose k}'                  15000
run_test "brace"                    '{n \brace k}'                   15000
run_test "brack"                    '{n \brack k}'                   15000

# =====================================================================
# SECTION 17: Sqrt and roots
# =====================================================================
echo "=== Sqrt and Roots ==="
run_test "sqrt simple"              '\sqrt{x}'                       15000
run_test "sqrt expression"          '\sqrt{x^2+y^2}'                 50000
run_test "sqrt nested"              '\sqrt{\sqrt{x}}'                 30000
run_test "root cube"                '\sqrt[3]{x}'                     20000
run_test "root nth"                 '\sqrt[n]{x+y}'                   40000
run_test "root command"             '\root 3 \of {x}'                 22000

# =====================================================================
# SECTION 18: Accents
# =====================================================================
echo "=== Accents ==="
run_test "accent hat"               '\hat{x}'                         3000
run_test "accent tilde"             '\tilde{x}'                       3000
run_test "accent bar"               '\bar{x}'                         3000
run_test "accent dot"               '\dot{x}'                         3000
run_test "accent ddot"              '\ddot{x}'                        3000
run_test "accent vec"               '\vec{x}'                         3000
run_test "accent acute"             '\acute{x}'                       3000
run_test "accent grave"             '\grave{x}'                       3000
run_test "accent breve"             '\breve{x}'                       3000
run_test "accent check"             '\check{x}'                       3000
run_test "accent mathring"          '\mathring{x}'                    3000
run_test "accent hat compound"      '\hat{xy}'                        5000
run_test "accent tilde compound"    '\tilde{xy}'                      5000
run_test "accent bar compound"      '\bar{xy}'                        5000
run_test "accent ddot compound"     '\ddot{xy}'                       5000
run_test "accent vec compound"      '\vec{xy}'                        5000

# =====================================================================
# SECTION 19: Wider accents (widehat, widetilde - in ACCENTS table)
# =====================================================================
echo "=== Wider Accents ==="
run_test "waccent widehat"          '\widehat{xy}'                    5000
run_test "waccent widehat long"     '\widehat{xyz}'                   5000
run_test "waccent widetilde"        '\widetilde{xy}'                  5000
run_test "waccent widetilde long"   '\widetilde{xyz}'                 5000

# =====================================================================
# SECTION 20: Text mode
# =====================================================================
echo "=== Text Mode ==="
run_test "text simple"              '\text{hello}'                    3000
run_test "text in formula"          'x + \text{and} + y'             10000
run_test "text mixed"               '\text{hello} \text{world}'       8000
run_test "mbox"                     '\mbox{hello world}'              5000
run_test "textnormal"               '\textnormal{hello}'              3000
run_test "textbf"                   '\textbf{bold}'                   1
run_test "textit"                   '\textit{italic}'                 3000
run_test "textsf"                   '\textsf{sans}'                   1
run_test "texttt"                   '\texttt{mono}'                   1
run_test "textsc"                   '\textsc{smallcaps}'              1
run_test "textsl"                   '\textsl{slanted}'                1
run_test "textup"                   '\textup{upright}'                3000
run_test "textmd"                   '\textmd{medium}'                 1

# =====================================================================
# SECTION 21: Font commands
# =====================================================================
echo "=== Font Commands ==="
run_test "font mathrm"              '\mathrm{sin}'                    3000
run_test "font mathbf"              '\mathbf{x}'                      3000
run_test "font mathbf compound"     '\mathbf{x+y}'                    8000
run_test "font mathit"              '\mathit{sin}'                    3000
run_test "font mathcal"             '\mathcal{ABC}'                   5000
run_test "font mathbb"              '\mathbb{R}'                      3000
run_test "font mathbb sup"          '\mathbb{R}^n'                    5000
run_test "font mathsf"              '\mathsf{ABC}'                    5000
run_test "font mathtt"              '\mathtt{ABC}'                    5000
run_test "font mathfrak"            '\mathfrak{ABC}'                  5000
run_test "font mathscr"             '\mathscr{ABC}'                   5000
run_test "font boldsymbol"          '\boldsymbol{\theta}'             5000
run_test "font pmb"                 '\pmb{x}'                         1
run_test "font mathnormal"          '\mathnormal{ABC}'                3000
run_test "font mathds"              '\mathds{R}'                      1
run_test "font Bbb"                 '\Bbb{R}'                         1
run_test "font frak"                '\frak{ABC}'                      1
run_test "font bf"                  '{\bf x}'                         1
run_test "font it"                  '{\it x}'                         1
run_test "font cal"                 '{\cal ABC}'                      1
run_test "font rm"                  '{\rm x}'                         1
run_test "font sf"                  '{\sf x}'                         1
run_test "font tt"                  '{\tt x}'                         1
run_test "font mit"                 '{\mit x}'                        1
run_test "font oldstyle"            '{\oldstyle 123}'                 1
run_test "font mathbfit"            '\mathbfit{x}'                    1
run_test "font mathbfsf"            '\mathbfsf{x}'                    1
run_test "font mathbfsc"            '\mathbfsc{x}'                    1
run_test "font mathbfcal"           '\mathbfcal{x}'                   1

# =====================================================================
# SECTION 22: Spacing
# =====================================================================
echo "=== Spacing ==="
run_test "space thin"               'x\,y'                            3000
run_test "space med"                'x\;y'                            3000
run_test "space thick"              'x\:y'                            3000
run_test "space negthin"            'x\!y'                            3000
run_test "space quad"               'x \quad y'                       3000
run_test "space qquad"              'x \qquad y'                      3000
run_test "space ens"                'x \enspace y'                    1
run_test "space thinspace"          'x \thinspace y'                  1
run_test "space medspace"           'x \medspace y'                   1
run_test "space thickspace"         'x \thickspace y'                 1
run_test "space negthinspace"       'x \negthinspace y'               1
run_test "space negmedspace"        'x \negmedspace y'                1
run_test "space negthickspace"      'x \negthickspace y'              1
run_test "space hspace"             'x \hspace{2em} y'                100
run_test "space hfill"              'x \hfill y'                      1
run_test "space hfil"               'x \hfil y'                       1
run_test "space hskip"              'x \hskip 2em y'                  1
run_test "space mkern"              'x \mkern 2mu y'                  1
run_test "space ms kip"             'x \mskip 2mu y'                  1
run_test "space kern"               'x \kern 2em y'                   1
run_test "space nobreakspace"       'x \nobreakspace y'               1

# =====================================================================
# SECTION 23: Matrix environments
# =====================================================================
echo "=== Matrix Environments ==="
run_test "matrix 2x2"               '\begin{matrix} 1 & 2 \\ 3 & 4 \end{matrix}'   40000
run_test "pmatrix 2x2"              '\begin{pmatrix} a & b \\ c & d \end{pmatrix}' 80000
run_test "bmatrix 2x2"              '\begin{bmatrix} a & b \\ c & d \end{bmatrix}' 80000
run_test "Bmatrix 2x2"              '\begin{Bmatrix} 1 & 2 \\ 3 & 4 \end{Bmatrix}' 50000
run_test "vmatrix 2x2"              '\begin{vmatrix} a & b \\ c & d \end{vmatrix}' 80000
run_test "Vmatrix 2x2"              '\begin{Vmatrix} a & b \\ c & d \end{Vmatrix}' 80000
run_test "matrix 3x3"               '\begin{matrix} a & b & c \\ d & e & f \\ g & h & i \end{matrix}' 50000
run_test "smallmatrix"              '\begin{smallmatrix} a & b \\ c & d \end{smallmatrix}' 1
run_test "array basic"              '\begin{array}{cc} a & b \\ c & d \end{array}' 1

# =====================================================================
# SECTION 24: Alignment environments
# =====================================================================
echo "=== Alignment Environments ==="
run_test "env gather"               '\begin{gathered} a \\ b \\ c \end{gathered}' 15000
run_test "env gather multi"         '\begin{gathered} a = b \\ c = d \\ e = f \end{gathered}' 20000
run_test "env align"                '\begin{aligned} a &= b \\ c &= d \end{aligned}' 20000
run_test "env align multi"          '\begin{aligned} x &= y + z & y &= w + t \\ a &= b & c &= d \end{aligned}' 30000
run_test "env alignat"              '\begin{alignedat}{2} a &= b & c &= d \end{alignedat}' 1
run_test "env split"                '\begin{split} a &= b \\ c &= d \end{split}' 20000
run_test "env multline"             '\begin{multline} a \\ b \\ c \end{multline}' 1
run_test "env flalign"              '\begin{flalign} a &= b \\ c &= d \end{flalign}' 1
run_test "env equation"             '\begin{equation} a = b \end{equation}' 1
run_test "env equation*"            '\begin{equation*} a = b \end{equation*}' 1
run_test "env gather star"          '\begin{gather*} a \\ b \end{gather*}' 1
run_test "env cases simple"         'f(x) = \begin{cases} 1 & x>0 \\ 0 & x \le 0 \end{cases}' 50000
run_test "env cases multi"          '|x| = \begin{cases} x & x\ge 0 \\ -x & x < 0 \end{cases}' 40000
run_test "env aligned"              '\begin{aligned} x &= y \\ z &= w \end{aligned}' 20000

# =====================================================================
# SECTION 25: Mod functions
# =====================================================================
echo "=== Mod Functions ==="
run_test "mod pmod"                 'x \equiv y \pmod{n}'            15000
run_test "mod bmod"                 'a \bmod b'                      10000
run_test "mod mod"                  'x \mod n'                       10000
run_test "mod pod"                  'x \pod{n}'                      10000

# =====================================================================
# SECTION 26: Limits and style commands
# =====================================================================
echo "=== Limits and Style ==="
run_test "style limits"             '\lim_{x\to 0} \frac{\sin x}{x}' 30000
run_test "style nolimits"           '\int\limits_0^1 x\,dx'          1
run_test "style displaystyle"       '\displaystyle\sum_{i=0}^\infty a_i' 40000
run_test "style textstyle"          '\textstyle\sum_{i=0}^n a_i'    20000
run_test "style scriptstyle"        '\scriptstyle x^2'                5000
run_test "style scriptscriptstyle"  '\scriptscriptstyle x^2'          5000

# =====================================================================
# SECTION 27: Color
# =====================================================================
echo "=== Color ==="
run_test "color simple"             '\color{red}{x}'                  3000
run_test "color multi"              '\color{blue}{x+y}'               5000
run_test "textcolor"                '\textcolor{blue}{hello}'         5000
run_test "colorbox"                 '\colorbox{yellow}{text}'          1
run_test "fcolorbox"                '\fcolorbox{red}{yellow}{text}'    12000
run_test "definecolor"              '\definecolor{mycolor}{rgb}{1,0,0} \color{mycolor}{x}' 1

# =====================================================================
# SECTION 28: Boxes
# =====================================================================
echo "=== Boxes ==="
run_test "boxed simple"             '\boxed{x+y}'                    15000
run_test "boxed fraction"           '\boxed{\frac{a}{b}}'            15000
run_test "boxed complex"            '\boxed{x^2 + y^2 = z^2}'        20000
run_test "fbox"                     '\fbox{x}'                       8000
run_test "framebox"                 '\framebox{x}'                   8000
run_test "mbox box"                 '\mbox{hello}'                   3000
run_test "hbox"                     '\hbox{hello}'                   1
run_test "makebox"                  '\makebox{text}'                 1
run_test "raisebox"                 '\raisebox{2pt}{text}'            1
run_test "rule"                     '\rule{1cm}{1cm}'                 1
run_test "phantom"                  '\phantom{x}'                    2000
run_test "phantom expr"             '\phantom{x+y}'                  3000
run_test "hphantom"                 '\hphantom{x}'                    1
run_test "vphantom"                 '\vphantom{x}'                    1
run_test "smash"                    '\smash{x}'                      2000
run_test "mathchoice"               '\mathchoice{x}{y}{z}{w}'        1
run_test "mathclap"                 '\sum_{\mathclap{i=1}^n} a_i'    1
run_test "mathllap"                 '\mathllap{x}'                   1
run_test "mathrlap"                 '\mathrlap{x}'                   1
run_test "mathmbox"                 '\mathmbox{x}'                   1
run_test "mathmakebox"              '\mathmakebox{x}'                1

# =====================================================================
# SECTION 29: Cancel
# =====================================================================
echo "=== Cancel ==="
run_test "cancel"                   '\cancel{x}'                     5000
run_test "cancel fraction"          '\cancel{\frac{x}{y}}'           15000
run_test "bcancel"                  '\bcancel{x}'                    5000
run_test "xcancel"                  '\xcancel{x}'                    5000
run_test "cancelto"                 '\cancelto{0}{x}'                 1

# =====================================================================
# SECTION 30: Extensible arrows (expanded set - same as section 9)
# =====================================================================
echo "=== Extensible Arrows (Expanded) ==="
run_test "xarrow2 xrightarrow"      '\xrightarrow{text}'             15000
run_test "xarrow2 xleftarrow"       '\xleftarrow{text}'              15000
run_test "xarrow2 xLeftarrow"       '\xLeftarrow{text}'              15000
run_test "xarrow2 xRightarrow"      '\xRightarrow{text}'             15000
run_test "xarrow2 xleftrightarrow"  '\xleftrightarrow{text}'         15000
run_test "xarrow2 xLeftrightarrow"  '\xLeftrightarrow{text}'         15000
run_test "xarrow2 xmapsto"          '\xmapsto{text}'                 15000
run_test "xarrow2 xhookleftarrow"   '\xhookleftarrow{text}'          15000
run_test "xarrow2 xhookrightarrow"  '\xhookrightarrow{text}'         15000
run_test "xarrow2 xrightharpoondown" '\xrightharpoondown{text}'      10000
run_test "xarrow2 xrightharpoonup"   '\xrightharpoonup{text}'        12000
run_test "xarrow2 xleftharpoondown"  '\xleftharpoondown{text}'       10000
run_test "xarrow2 xleftharpoonup"    '\xleftharpoonup{text}'         12000
run_test "xarrow2 xrightleftharpoons" '\xrightleftharpoons{text}'    1
run_test "xarrow2 xleftrightharpoons" '\xleftrightharpoons{text}'    1
run_test "xarrow2 xtwoheadleftarrow"  '\xtwoheadleftarrow{text}'     1
run_test "xarrow2 xtwoheadrightarrow" '\xtwoheadrightarrow{text}'    1

# =====================================================================
# SECTION 31: AMS environments
# =====================================================================
echo "=== AMS Environments ==="
run_test "amsenv align"             '\begin{aligned} a &= b \\ c &= d \end{aligned}' 20000
run_test "amsenv gather"            '\begin{gathered} a \\ b \\ c \end{gathered}' 15000
run_test "amsenv split"             '\begin{split} a &= b \end{split}' 20000
run_test "amsenv multline"          '\begin{multline} a \\ b \end{multline}' 1
run_test "amsenv alignat"           '\begin{alignedat}{2} a &= b & c &= d \end{alignedat}' 1
run_test "amsenv flalign"           '\begin{flalign} a &= b \end{flalign}' 1

# =====================================================================
# SECTION 32: Operatorname
# =====================================================================
echo "=== Operatorname ==="
run_test "operatorname"             '\operatorname{argmax}_x'        15000
run_test "operatorname star"        '\operatorname*{argmax}_{x\in X}' 15000
run_test "operatorname multi"       '\operatorname{min}_{x} f(x)'    15000
run_test "operatorname long"        '\operatorname{verylongname}_x'  15000
run_test "DeclareMathOperator"      '\DeclareMathOperator{\adj}{adj} \adj(A)' 10000
run_test "DeclareMathOperator star" '\DeclareMathOperator*{\myop}{myop} \myop_{x=1}^n' 20000

# =====================================================================
# SECTION 33: Misc commands (left, right, middle, buildrel, etc.)
# =====================================================================
echo "=== Misc Commands ==="
run_test "left right"               '\left( \frac{x}{y} \right)'    10000
run_test "left right nested"        '\left( x + \left[ y + z \right] \right)' 15000
run_test "left ."                   '\left. \frac{x}{y} \right|'    15000
run_test "middle"                   '\left\{ x \middle| y \right\}' 1
run_test "buildrel"                 '\buildrel{*}\over{\rightarrow}' 1
run_test "stackrel test"            '\stackrel{*}{X}'                1
run_test "atopwithdelims"           '{x \atopwithdelims ( ) y}'     1
run_test "abovewithdelims"          '{x \abovewithdelims ( ) 2pt y}' 1

# =====================================================================
# SECTION 34: Superscript/Subscript
# =====================================================================
echo "=== Superscript/Subscript ==="
run_test "sup simple"               'x^2'                             3000
run_test "sub simple"               'x_i'                             2000
run_test "sup sub"                  'x_i^2'                           3000
run_test "sub sup"                  'x^2_i'                           3000
run_test "sup compound"             'x^{n+1}'                         5000
run_test "sub compound"             'x_{i+j}'                         5000
run_test "sup command"              'x^\infty'                        3000
run_test "sub command"              'x_\alpha'                        3000
run_test "nested scripts"           'x^{y^{z}}'                       5000
run_test "multi scripts"            'x_i^j^k'                         5000
run_test "subsuper"                 '\int_0^1 x^2 \,dx'              40000
run_test "scripts on largeop"       '\sum_{i=1}^n a_i'               40000

# =====================================================================
# SECTION 35: Substack
# =====================================================================
echo "=== Substack ==="
run_test "substack"                 '\sum_{\substack{0<i<n\\j>0}} a_{ij}' 30000
run_test "substack multi"           '\prod_{\substack{i\ge0\\j\le n}} x_{ij}' 30000

# =====================================================================
# SECTION 36: Negation (\not)
# =====================================================================
echo "=== Negation ==="
run_test "not ="                    'x \not= y'                       5000
run_test "not equiv"                'a \not\equiv b'                  5000
run_test "not <"                    'a \not< b'                       5000
run_test "not >"                    'a \not> b'                       5000
run_test "not |"                    'a \not| b'                       5000
run_test "not in"                   'x \notin A'                      5000

# =====================================================================
# SECTION 37: Dots
# =====================================================================
echo "=== Dots ==="
run_test "dots ldots"               '1, 2, \ldots, n'                10000
run_test "dots cdots"               'x_1 + x_2 + \cdots + x_n'       10000
run_test "dots vdots"               'a \vdots b'                      5000
run_test "dots ddots"               '\ddots'                          2000
run_test "dots dots"                '1,2,\dots,n'                     8000

# =====================================================================
# SECTION 38: Implied by / Implies / Iff
# =====================================================================
echo "=== Logical Symbols ==="
run_test "logical implies"          'a \implies b'                    5000
run_test "logical impliedby"        'a \impliedby b'                  5000
run_test "logical iff"              'a \iff b'                        5000
run_test "logical land"             'a \land b'                       1
run_test "logical lor"              'a \lor b'                        1

# =====================================================================
# SECTION 39: Complex expressions (integration tests)
# =====================================================================
echo "=== Complex Expressions ==="
run_test "complex quadratic"        '\frac{-b \pm \sqrt{b^2 - 4ac}}{2a}'  50000
run_test "complex taylor"           '\sum_{n=0}^{\infty} \frac{f^{(n)}(a)}{n!} (x-a)^n' 80000
run_test "complex matrix"           '\begin{pmatrix} a & b \\ c & d \end{pmatrix}^{-1} = \frac{1}{ad-bc} \begin{pmatrix} d & -b \\ -c & a \end{pmatrix}' 100000
run_test "complex integral"         '\oint_C \vec{F} \cdot d\vec{r} = \iint_S (\nabla \times \vec{F}) \cdot d\vec{S}' 100000
run_test "complex trig"             '\sin^2 \theta + \cos^2 \theta = 1' 30000
run_test "complex frac exp"         'e^{\frac{x}{y}}'                  9000
run_test "complex greek formula"    '\Gamma(z) = \int_0^\infty t^{z-1} e^{-t}\,dt'  60000
run_test "complex binomial"         '\sum_{k=0}^n \binom{n}{k} x^k y^{n-k}'  50000
run_test "complex lim"              '\lim_{n\to\infty} \left(1 + \frac{1}{n}\right)^n = e' 60000
run_test "complex matrix inverse"   'A^{-1} = \frac{1}{\det A} \operatorname{adj} A'  50000
run_test "complex cases"            'f(x) = \begin{cases} x^2 & x \ge 0 \\ 0 & x < 0 \end{cases}' 50000
run_test "complex over under"       '\overline{\int_a^b f(x)\,dx} = \underline{\int_a^b f(x)\,dx}' 50000
run_test "complex color formula"    '\color{red}{\sum_{i=1}^n i} = \color{blue}{\frac{n(n+1)}{2}}' 50000

# =====================================================================
# SECTION 40: Greek with all variants and punctuation patterns
# =====================================================================
echo "=== Greek Patterns ==="
run_test "pattern alpha beta"       '\alpha \beta \gamma \delta'      10000
run_test "pattern theta variants"   '\theta \vartheta \Theta \varTheta' 10000
run_test "pattern phi variants"     '\phi \varphi \Phi \varPhi'       10000
run_test "pattern sigma variants"   '\sigma \varsigma \Sigma \varSigma' 10000
run_test "pattern epsilon variants" '\epsilon \varepsilon'             5000
run_test "pattern pi variants"      '\pi \varpi \Pi \varPi'           10000
run_test "pattern rho variants"     '\rho \varrho'                    5000
run_test "pattern kappa"            '\kappa'                          2000

# =====================================================================
# SECTION 41: Edge cases
# =====================================================================
echo "=== Edge Cases ==="
run_test "edge empty"               ''                                   1
run_test "edge single letter"       'x'                               2000
run_test "edge single number"       '1'                               2000
run_test "edge single operator"     '+'                               2000
run_test "edge text only"           '\text{hello}'                    3000
run_test "edge nested braces"       '{{x}}'                           2000
run_test "edge deeply nested"       '\sqrt{\sqrt{\sqrt{x}}}'          30000
run_test "edge frac frac"           '\frac{\frac{a}{b}}{\frac{c}{d}}'  9000
run_test "edge super super"         'x^{y^{z^{w}}}'                   5000
run_test "edge sub sub"             'x_{i_{j_k}}'                     5000
run_test "edge super sub combo"     '\int_{0}^{1} \int_{0}^{2} f(x,y)\,dx\,dy' 50000
run_test "edge many operators"      'x + y - z * w / v'               5000
run_test "edge frac in script"      'x^{\frac{1}{2}}'                 10000

# =====================================================================
# SECTION 42: Recent layout regressions
# These are smoke tests for bugs that previously rendered misplaced,
# blurry, or malformed output.
# =====================================================================
echo "=== Recent Layout Regressions ==="
run_test "regression binom fixed fence"       '\binom{n}{k}'                         18000
run_test "regression genfrac fixed fence"     '\genfrac{(}{)}{0pt}{}{n}{k}'          18000
run_test "regression choose fixed fence"      '{n \choose k}'                        18000
run_test "regression binomial theorem"        '(x+y)^n=\sum_{k=0}^{n}\binom{n}{k}x^{n-k}y^k' 180000
run_test "regression quadratic radical frac"  'x=\frac{-b\pm\sqrt{b^2-4ac}}{2a}'    120000
run_test "regression radical polynomial"      '\sqrt{b^2-4ac}'                      48000
run_test "regression radical fraction"        '\sqrt{\frac{1}{1+x^2}}'              76000
run_test "regression radical delimiters"      '\left(\sqrt{x^2+1}\right)'           82000
run_test "regression dense scripts"           'x^{n-k}y^k'                          25000
run_test "regression sum binom limits"        '\sum_{k=0}^{n}\binom{n}{k}'           46000
run_test "regression cube root fraction"      '\sqrt[3]{\frac{x+1}{y-1}}'           82000
run_test "regression named operator spacing"  '\inf_{n} a_n \min_{x\in X} f(x)'     55000
run_test "regression matrix fence superscript" '\left[\begin{matrix}a&b\\c&d\end{matrix}\right]^2' 45000
run_test "regression array colspec skipped"   '\left\{\begin{array}{cc}a&b\\c&d\end{array}\right\}_i^j' 45000
run_test "regression cases brace height"      '\begin{cases}x^2,&x>0\\-x,&x\le0\end{cases}' 65000
run_test "regression box frame commands"      '\boxed{x+1}\quad\fbox{x+1}\quad\framebox{x+1}\quad\fcolorbox{red}{yellow}{x+1}\quad\bbox[border:1px solid red]{x+1}' 95000
run_test "regression commutative diagram arrows" '\begin{matrix}A&\xrightarrow{f}&B\\\downarrow&&\downarrow\\C&\xrightarrow{g}&D\end{matrix}' 60000
run_test "regression explicit limits modes"   '\sum\nolimits_{i=1}^{n}a_i\quad\int\limits_0^1 x^2\,dx' 90000

# =====================================================================
# SECTION 43: All remaining MathJax commands from documentation
# Not yet implemented - min_size=1 to pass trivially
# =====================================================================
echo "=== Additional MathJax Commands (not yet implemented) ==="
run_test "extra acute"              '\acute{x}'                       3000
run_test "extra AllowBreak"         '\allowbreak'                     1
run_test "extra And"                '\And'                            1
run_test "extra angle"              '\angle'                          2000
run_test "extra approx"             '\approx'                         5000
run_test "extra araay"              '\begin{array}{c} x \end{array}'   1
run_test "extra ast"                '\ast'                            2000
run_test "extra asymp"              '\asymp'                          5000
run_test "extra backepsilon"        '\backepsilon'                    2000
run_test "extra backprime"          '\backprime'                      1
run_test "extra backsim"            '\backsim'                        3000
run_test "extra backsimeq"          '\backsimeq'                      4000
run_test "extra backslash"          '\backslash'                      2000
run_test "extra bar"                '\bar{x}'                         3000
run_test "extra Bbb"                '\Bbb{R}'                         1
run_test "extra Bbbk"               '\Bbbk'                           2000
run_test "extra because"            '\because'                        5000
run_test "extra beta"               '\beta'                           2000
run_test "extra beth"               '\beth'                           1
run_test "extra between"            '\between'                        5000
run_test "extra bf"                 '{\bf x}'                         1
run_test "extra bigcap"             '\bigcap'                         2000
run_test "extra bigcirc"            '\bigcirc'                        1
run_test "extra bigcup"             '\bigcup'                         2000
run_test "extra bigodot"            '\bigodot'                        2000
run_test "extra bigoplus"           '\bigoplus'                       2000
run_test "extra bigotimes"          '\bigotimes'                      2000
run_test "extra bigsqcup"           '\bigsqcup'                       2000
run_test "extra bigstar"            '\bigstar'                        1
run_test "extra bigtriangledown"    '\bigtriangledown'                2000
run_test "extra bigtriangleup"      '\bigtriangleup'                  2000
run_test "extra biguplus"           '\biguplus'                       2000
run_test "extra bigvee"             '\bigvee'                         2000
run_test "extra bigwedge"           '\bigwedge'                       2000
run_test "extra blacklozenge"       '\blacklozenge'                   1
run_test "extra blacksquare"        '\blacksquare'                    2000
run_test "extra blacktriangle"      '\blacktriangle'                  2000
run_test "extra blacktriangledown"  '\blacktriangledown'              2000
run_test "extra blacktriangleleft"  '\blacktriangleleft'              1
run_test "extra blacktriangleright" '\blacktriangleright'             1
run_test "extra bot"                '\bot'                            2000
run_test "extra bowtie"             '\bowtie'                         2000
run_test "extra boxdot"             '\boxdot'                         5000
run_test "extra boxminus"           '\boxminus'                       5000
run_test "extra boxplus"            '\boxplus'                        5000
run_test "extra boxtimes"           '\boxtimes'                       5000
run_test "extra breve"              '\breve{x}'                       3000
run_test "extra bullet"             '\bullet'                         2000
run_test "extra bumpeq"             '\bumpeq'                         5000
run_test "extra Bumpeq"             '\Bumpeq'                         5000
run_test "extra cal"                '{\cal ABC}'                      1
run_test "extra Cap"                '\Cap'                            1
run_test "extra check"              '\check{x}'                       3000
run_test "extra chi"                '\chi'                            2000
run_test "extra circ"               '\circ'                           2000
run_test "extra circeq"             '\circeq'                         1
run_test "extra circlearrowleft"    '\circlearrowleft'                1
run_test "extra circlearrowright"   '\circlearrowright'               1
run_test "extra circledast"         '\circledast'                     5000
run_test "extra circledcirc"        '\circledcirc'                    5000
run_test "extra circleddash"        '\circleddash'                    5000
run_test "extra circledR"           '\circledR'                       2000
run_test "extra circledS"           '\circledS'                       2000
run_test "extra clubsuit"           '\clubsuit'                       2000
run_test "extra colon"              '\colon'                          2000
run_test "extra complement"         '\complement'                     1
run_test "extra cong"               '\cong'                           5000
run_test "extra coprod"             '\coprod'                         2000
run_test "extra Cup"                '\Cup'                            1
run_test "extra curlyeqprec"        '\curlyeqprec'                    1
run_test "extra curlyeqsucc"        '\curlyeqsucc'                    1
run_test "extra curlyvee"           '\curlyvee'                       5000
run_test "extra curlywedge"         '\curlywedge'                     5000
run_test "extra curvearrowleft"     '\curvearrowleft'                 1
run_test "extra curvearrowright"    '\curvearrowright'                1
run_test "extra dagger"             '\dagger'                         2000
run_test "extra daleth"             '\daleth'                         1
run_test "extra dashv"              '\dashv'                          5000
run_test "extra ddagger"            '\ddagger'                        2000
run_test "extra dddot"              '\dddot{x}'                       1
run_test "extra ddddot"             '\ddddot{x}'                      1
run_test "extra deg"                '\deg'                            5000
run_test "extra Delta"              '\Delta'                          2000
run_test "extra delta"              '\delta'                          2000
run_test "extra det"                '\det'                            5000
run_test "extra diagdown"           '\diagdown'                       1
run_test "extra diagup"             '\diagup'                         1
run_test "extra diamond"            '\diamond'                        2000
run_test "extra Diamond"            '\Diamond'                        2000
run_test "extra diamondsuit"        '\diamondsuit'                    2000
run_test "extra digamma"            '\digamma'                        2000
run_test "extra dim"                '\dim'                            5000
run_test "extra displaylines"       '\displaylines{x\\y}'             1
run_test "extra div"                '\div'                            2000
run_test "extra doteq"              '\doteq'                          5000
run_test "extra doteqdot"           '\doteqdot'                       5000
run_test "extra dotplus"            '\dotplus'                        5000
run_test "extra dots"               '\dots'                           2000
run_test "extra dotsb"              '\dotsb'                          2000
run_test "extra dotsc"              '\dotsc'                          2000
run_test "extra dotsi"              '\dotsi'                          2000
run_test "extra dotsm"              '\dotsm'                          2000
run_test "extra dotso"              '\dotso'                          2000
run_test "extra doublebarwedge"     '\doublebarwedge'                 1
run_test "extra doublecap"          '\doublecap'                      1
run_test "extra doublecup"          '\doublecup'                      1
run_test "extra downarrow"          '\downarrow'                      2000
run_test "extra Downarrow"          '\Downarrow'                      2000
run_test "extra downdownarrows"     '\downdownarrows'                 1
run_test "extra downharpoonleft"    '\downharpoonleft'                1
run_test "extra downharpoonright"   '\downharpoonright'               1
run_test "extra ell"                '\ell'                            2000
run_test "extra emptyset"           '\emptyset'                       2000
run_test "extra enspace"            'x \enspace y'                    1
run_test "extra epsilon"            '\epsilon'                        2000
run_test "extra eqcirc"             '\eqcirc'                         4000
run_test "extra eqsim"              '\eqsim'                          4000
run_test "extra eqslantgtr"         '\eqslantgtr'                     1
run_test "extra eqslantless"        '\eqslantless'                    1
run_test "extra equiv"              '\equiv'                          5000
run_test "extra eta"                '\eta'                            2000
run_test "extra eth"                '\eth'                            1
run_test "extra exists"             '\exists'                         2000
run_test "extra exp"                '\exp'                            5000
run_test "extra fallingdotseq"      '\fallingdotseq'                  5000
run_test "extra flat"               '\flat'                           1
run_test "extra forall"             '\forall'                         2000
run_test "extra frak"               '{\frak ABC}'                     1
run_test "extra frown"              '\frown'                          2000
run_test "extra Game"               '\Game'                           2000
run_test "extra gamma"              '\gamma'                          2000
run_test "extra Gamma"              '\Gamma'                          2000
run_test "extra ge"                 '\ge'                             5000
run_test "extra geq"                '\geq'                            5000
run_test "extra geqq"               '\geqq'                           1
run_test "extra geqslant"           '\geqslant'                       1
run_test "extra gets"               '\gets'                           5000
run_test "extra gg"                 '\gg'                             5000
run_test "extra ggg"                '\ggg'                            1
run_test "extra gimel"              '\gimel'                          1
run_test "extra gnapprox"           '\gnapprox'                       1
run_test "extra gneq"               '\gneq'                           1
run_test "extra gneqq"              '\gneqq'                          1
run_test "extra gnsim"              '\gnsim'                          1
run_test "extra grave"              '\grave{x}'                       3000
run_test "extra gtrapprox"          '\gtrapprox'                      1
run_test "extra gtrdot"             '\gtrdot'                         1
run_test "extra gtreqless"          '\gtreqless'                      1
run_test "extra gtreqqless"         '\gtreqqless'                     1
run_test "extra gtrless"            '\gtrless'                        1
run_test "extra gtrsim"             '\gtrsim'                         1
run_test "extra gvertneqq"          '\gvertneqq'                      1
run_test "extra hat"                '\hat{x}'                         3000
run_test "extra hbar"               '\hbar'                           2000
run_test "extra heartsuit"          '\heartsuit'                      2000
run_test "extra hfil"               '\hfil'                           1
run_test "extra hfill"              '\hfill'                          1
run_test "extra hom"                '\hom'                            5000
run_test "extra hookleftarrow"      '\hookleftarrow'                  3000
run_test "extra hookrightarrow"     '\hookrightarrow'                 3000
run_test "extra hphantom"           '\hphantom{x}'                    1
run_test "extra hslash"             '\hslash'                         2000
run_test "extra hspace"             '\hspace{1em}'                    100
run_test "extra iddots"             '\iddots'                         1
run_test "extra idotsint"           '\idotsint'                       18000
run_test "extra iff"                '\iff'                            5000
run_test "extra iiiint"             '\iiiint'                         10000
run_test "extra iiint"              '\iiint'                          15000
run_test "extra iint"               '\iint'                           10000
run_test "extra Im"                 '\Im'                             2000
run_test "extra imath"              '\imath'                          2000
run_test "extra impliedby"          '\impliedby'                      5000
run_test "extra implies"            '\implies'                        5000
run_test "extra in"                 '\in'                             5000
run_test "extra inf"                '\inf'                            5000
run_test "extra infty"              '\infty'                          2000
run_test "extra injlim"             '\injlim'                         15000
run_test "extra int"                '\int'                            2000
run_test "extra intercal"           '\intercal'                       4000
run_test "extra intop"              '\intop'                          2000
run_test "extra iota"               '\iota'                           2000
run_test "extra it"                 '{\it x}'                         1
run_test "extra jmath"              '\jmath'                          2000
run_test "extra Join"               '\Join'                           1
run_test "extra kappa"              '\kappa'                          2000
run_test "extra ker"                '\ker'                            5000
run_test "extra kern"               '\kern 1em x'                     1
run_test "extra lambda"             '\lambda'                         2000
run_test "extra Lambda"             '\Lambda'                         2000
run_test "extra land"               '\land'                           1
run_test "extra langle"             '\langle'                         2000
run_test "extra lbrace"             '\lbrace'                         2000
run_test "extra lbrack"             '\lbrack'                         2000
run_test "extra lceil"              '\lceil'                          2000
run_test "extra ldots"              '\ldots'                          2000
run_test "extra le"                 '\le'                             5000
run_test "extra leadsto"            '\leadsto'                        1
run_test "extra leftarrow"          '\leftarrow'                      3000
run_test "extra Leftarrow"          '\Leftarrow'                      3000
run_test "extra leftarrowtail"      '\leftarrowtail'                  1
run_test "extra leftharpoondown"    '\leftharpoondown'                3000
run_test "extra leftharpoonup"      '\leftharpoonup'                  3000
run_test "extra leftleftarrows"     '\leftleftarrows'                 1
run_test "extra leftrightarrow"     '\leftrightarrow'                 5000
run_test "extra Leftrightarrow"     '\Leftrightarrow'                 5000
run_test "extra leftrightarrows"    '\leftrightarrows'                1
run_test "extra leftrightharpoons"  '\leftrightharpoons'              5000
run_test "extra leftrightsquigarrow" '\leftrightsquigarrow'           1
run_test "extra leftthreetimes"     '\leftthreetimes'                 5000
run_test "extra leq"                '\leq'                            5000
run_test "extra leqq"               '\leqq'                           1
run_test "extra leqslant"           '\leqslant'                       1
run_test "extra lessapprox"         '\lessapprox'                     1
run_test "extra lessdot"            '\lessdot'                        1
run_test "extra lesseqgtr"          '\lesseqgtr'                      1
run_test "extra lesseqqgtr"         '\lesseqqgtr'                     1
run_test "extra lessgtr"            '\lessgtr'                        1
run_test "extra lesssim"            '\lesssim'                        1
run_test "extra lfloor"             '\lfloor'                         2000
run_test "extra lg"                 '\lg'                             5000
run_test "extra lgroup"             '\lgroup'                         2000
run_test "extra lhd"                '\lhd'                            1
run_test "extra lim"                '\lim'                            5000
run_test "extra liminf"             '\liminf'                         15000
run_test "extra limits"             '\limits'                         10
run_test "extra limsup"             '\limsup'                         15000
run_test "extra ll"                 '\ll'                             5000
run_test "extra llcorner"           '\llcorner'                       2000
run_test "extra Lleftarrow"         '\Lleftarrow'                     1
run_test "extra lll"                '\lll'                            1
run_test "extra lmoustache"         '\lmoustache'                     2000
run_test "extra ln"                 '\ln'                             5000
run_test "extra lnapprox"           '\lnapprox'                       1
run_test "extra lneq"               '\lneq'                           1
run_test "extra lneqq"              '\lneqq'                          1
run_test "extra lnot"               '\lnot'                           2000
run_test "extra lnsim"              '\lnsim'                          1
run_test "extra log"                '\log'                            5000
run_test "extra longleftarrow"      '\longleftarrow'                  5000
run_test "extra Longleftarrow"      '\Longleftarrow'                  5000
run_test "extra longleftrightarrow" '\longleftrightarrow'             5000
run_test "extra Longleftrightarrow" '\Longleftrightarrow'             5000
run_test "extra longmapsto"         '\longmapsto'                     5000
run_test "extra longrightarrow"     '\longrightarrow'                 5000
run_test "extra Longrightarrow"     '\Longrightarrow'                 5000
run_test "extra looparrowleft"      '\looparrowleft'                  1
run_test "extra looparrowright"     '\looparrowright'                 1
run_test "extra lor"                '\lor'                            1
run_test "extra lozenge"            '\lozenge'                        2000
run_test "extra lrcorner"           '\lrcorner'                       2000
run_test "extra Lsh"                '\Lsh'                            1
run_test "extra ltimes"             '\ltimes'                         5000
run_test "extra lvert"              '\lvert'                          2000
run_test "extra lVert"              '\lVert'                          2000
run_test "extra lvertneqq"          '\lvertneqq'                      1
run_test "extra maltese"            '\maltese'                        2000
run_test "extra mapsto"             '\mapsto'                         3000
run_test "extra mathring"           '\mathring{x}'                    3000
run_test "extra max"                '\max'                            5000
run_test "extra measuredangle"      '\measuredangle'                  2000
run_test "extra mho"                '\mho'                            1
run_test "extra mid"                '\mid'                            3000
run_test "extra min"                '\min'                            5000
run_test "extra mit"                '{\mit x}'                        1
run_test "extra mkern"              '\mkern 2mu x'                    1
run_test "extra models"             '\models'                         5000
run_test "extra mp"                 '\mp'                             5000
run_test "extra ms kip"             '\mskip 2mu x'                    1
run_test "extra mu"                 '\mu'                             2000
run_test "extra multimap"           '\multimap'                       1
run_test "extra nabla"              '\nabla'                          2000
run_test "extra natural"            '\natural'                        1
run_test "extra nearrow"            '\nearrow'                        2000
run_test "extra neg"                '\neg'                            2000
run_test "extra neq"                '\neq'                            5000
run_test "extra nexists"            '\nexists'                        2000
run_test "extra ngeq"               '\ngeq'                           1
run_test "extra ngeqq"              '\ngeqq'                          1
run_test "extra ngeqslant"          '\ngeqslant'                      1
run_test "extra ngtr"               '\ngtr'                           1
run_test "extra ni"                 '\ni'                             5000
run_test "extra nleftarrow"         '\nleftarrow'                     1
run_test "extra nLeftarrow"         '\nLeftarrow'                     1
run_test "extra nleftrightarrow"    '\nleftrightarrow'                1
run_test "extra nLeftrightarrow"    '\nLeftrightarrow'                1
run_test "extra nleq"               '\nleq'                           1
run_test "extra nleqq"              '\nleqq'                          1
run_test "extra nleqslant"          '\nleqslant'                      1
run_test "extra nless"              '\nless'                          1
run_test "extra nmid"               '\nmid'                           5000
run_test "extra nobreakspace"       '\nobreakspace'                   1
run_test "extra nolimits"           '\nolimits'                       10
run_test "extra not"                '\not ='                          5000
run_test "extra notin"              '\notin'                          5000
run_test "extra nparallel"          '\nparallel'                      5000
run_test "extra nprec"              '\nprec'                          1
run_test "extra npreceq"            '\npreceq'                        1
run_test "extra nrightarrow"        '\nrightarrow'                    1
run_test "extra nRightarrow"        '\nRightarrow'                    1
run_test "extra nsim"               '\nsim'                           1
run_test "extra nsubseteq"          '\nsubseteq'                      1
run_test "extra nsubseteqq"         '\nsubseteqq'                     1
run_test "extra nsucc"              '\nsucc'                          1
run_test "extra nsucceq"            '\nsucceq'                        1
run_test "extra nsupseteq"          '\nsupseteq'                      1
run_test "extra nsupseteqq"         '\nsupseteqq'                     1
run_test "extra ntriangleleft"      '\ntriangleleft'                  1
run_test "extra ntrianglelefteq"    '\ntrianglelefteq'                1
run_test "extra ntriangleright"     '\ntriangleright'                 1
run_test "extra ntrianglerighteq"   '\ntrianglerighteq'               1
run_test "extra nu"                 '\nu'                             2000
run_test "extra nvdash"             '\nvdash'                         1
run_test "extra nvDash"             '\nvDash'                         1
run_test "extra nVdash"             '\nVdash'                         1
run_test "extra nVDash"             '\nVDash'                         1
run_test "extra nwarrow"            '\nwarrow'                        2000
run_test "extra odot"               '\odot'                           5000
run_test "extra oiint"              '\oiint'                          1
run_test "extra oiiint"             '\oiiint'                         1
run_test "extra oint"               '\oint'                           2000
run_test "extra ointop"             '\ointop'                         2000
run_test "extra oldstyle"           '{\oldstyle 123}'                 1
run_test "extra omega"              '\omega'                          2000
run_test "extra Omega"              '\Omega'                          2000
run_test "extra omicron"            '\omicron'                        2000
run_test "extra ominus"             '\ominus'                         5000
run_test "extra oplus"              '\oplus'                          5000
run_test "extra oslash"             '\oslash'                         5000
run_test "extra otimes"             '\otimes'                         5000
run_test "extra overbrace"          '\overbrace{x+y}'                 10000
run_test "extra overleftarrow"      '\overleftarrow{abc}'             1
run_test "extra overleftrightarrow" '\overleftrightarrow{abc}'        1
run_test "extra overline"           '\overline{x}'                     6000
run_test "extra overparen"          '\overparen{abc}'                 1
run_test "extra overrightarrow"     '\overrightarrow{abc}'            1
run_test "extra overset"            '\overset{*}{X}'                  10000
run_test "extra own"                '\owns'                           5000
run_test "extra parallel"           '\parallel'                       5000
run_test "extra partial"            '\partial'                        2000
run_test "extra perp"               '\perp'                           5000
run_test "extra phi"                '\phi'                            2000
run_test "extra Phi"                '\Phi'                            2000
run_test "extra pi"                 '\pi'                             2000
run_test "extra Pi"                 '\Pi'                             2000
run_test "extra pitchfork"          '\pitchfork'                      5000
run_test "extra pm"                 '\pm'                             5000
run_test "extra pmod"               '\pmod{n}'                        10000
run_test "extra pod"                '\pod{n}'                         10000
run_test "extra Pr"                 '\Pr'                             5000
run_test "extra prec"               '\prec'                           5000
run_test "extra precapprox"         '\precapprox'                     1
run_test "extra preccurlyeq"        '\preccurlyeq'                    1
run_test "extra preceq"             '\preceq'                         5000
run_test "extra precnapprox"        '\precnapprox'                    1
run_test "extra precneqq"           '\precneqq'                       1
run_test "extra precnsim"           '\precnsim'                       1
run_test "extra precsim"            '\precsim'                        1
run_test "extra prime"              '\prime'                          2000
run_test "extra prod"               '\prod'                           2000
run_test "extra projlim"            '\projlim'                        15000
run_test "extra propto"             '\propto'                         4000
run_test "extra psi"                '\psi'                            2000
run_test "extra Psi"                '\Psi'                            2000
run_test "extra qquad"              '\qquad'                          3000
run_test "extra quad"               '\quad'                           3000
run_test "extra rangle"             '\rangle'                         2000
run_test "extra rbrace"             '\rbrace'                         2000
run_test "extra rbrack"             '\rbrack'                         2000
run_test "extra rceil"              '\rceil'                          2000
run_test "extra Re"                 '\Re'                             2000
run_test "extra ref"                '\ref{foo}'                       1
run_test "extra rfloor"             '\rfloor'                         2000
run_test "extra rgroup"             '\rgroup'                         2000
run_test "extra rhd"                '\rhd'                            1
run_test "extra rho"                '\rho'                            2000
run_test "extra rightarrow"         '\rightarrow'                     3000
run_test "extra Rightarrow"         '\Rightarrow'                     3000
run_test "extra rightarrowtail"     '\rightarrowtail'                 1
run_test "extra rightharpoondown"   '\rightharpoondown'               3000
run_test "extra rightharpoonup"     '\rightharpoonup'                 3000
run_test "extra rightleftarrows"    '\rightleftarrows'                1
run_test "extra rightleftharpoons"  '\rightleftharpoons'              5000
run_test "extra rightrightarrows"   '\rightrightarrows'               1
run_test "extra rightsquigarrow"    '\rightsquigarrow'                1
run_test "extra rightthreetimes"    '\rightthreetimes'                5000
run_test "extra risingdotseq"       '\risingdotseq'                   5000
run_test "extra rlap"               '\rlap{x}'                        1
run_test "extra rm"                 '{\rm x}'                         1
run_test "extra rmoustache"         '\rmoustache'                     2000
run_test "extra root"               '\root n \of x'                   22000
run_test "extra Rrightarrow"        '\Rrightarrow'                    1
run_test "extra Rsh"                '\Rsh'                            1
run_test "extra rtimes"             '\rtimes'                         5000
run_test "extra rule"               '\rule{1em}{1em}'                 1
run_test "extra rvert"              '\rvert'                          2000
run_test "extra rVert"              '\rVert'                          2000
run_test "extra searrow"            '\searrow'                        2000
run_test "extra sec"                '\sec'                            5000
run_test "extra setminus"           '\setminus'                       5000
run_test "extra sf"                 '{\sf x}'                         1
run_test "extra sharp"              '\sharp'                          1
run_test "extra shortmid"           '\shortmid'                       1
run_test "extra shortparallel"      '\shortparallel'                  1
run_test "extra sigma"              '\sigma'                          2000
run_test "extra Sigma"              '\Sigma'                          2000
run_test "extra sim"                '\sim'                            3000
run_test "extra simeq"              '\simeq'                          4000
run_test "extra sin"                '\sin'                            5000
run_test "extra sinh"               '\sinh'                           5000
run_test "extra smallint"           '\smallint'                       2000
run_test "extra smallsetminus"      '\smallsetminus'                  1
run_test "extra smallsmile"         '\smallsmile'                     1
run_test "extra smallfrown"         '\smallfrown'                     1
run_test "extra smile"              '\smile'                          2000
run_test "extra spadesuit"          '\spadesuit'                      2000
run_test "extra sphericalangle"      '\sphericalangle'                1
run_test "extra sqcap"              '\sqcap'                          5000
run_test "extra sqcup"              '\sqcup'                          5000
run_test "extra sqsubset"           '\sqsubset'                       5000
run_test "extra sqsubseteq"         '\sqsubseteq'                     5000
run_test "extra sqsupset"           '\sqsupset'                       5000
run_test "extra sqsupseteq"         '\sqsupseteq'                     5000
run_test "extra square"             '\square'                         2000
run_test "extra stackrel"           '\stackrel{*}{X}'                 1
run_test "extra star"               '\star'                           2000
run_test "extra strut"              '\strut'                          1
run_test "extra subset"             '\subset'                         5000
run_test "extra Subset"             '\Subset'                         1
run_test "extra subseteq"           '\subseteq'                       5000
run_test "extra subseteqq"          '\subseteqq'                      1
run_test "extra subsetneq"          '\subsetneq'                      5000
run_test "extra subsetneqq"         '\subsetneqq'                     1
run_test "extra substack"           '\substack{x\\y}'                 8000
run_test "extra succ"               '\succ'                           5000
run_test "extra succapprox"         '\succapprox'                     1
run_test "extra succcurlyeq"        '\succcurlyeq'                    1
run_test "extra succeq"             '\succeq'                         5000
run_test "extra succnapprox"        '\succnapprox'                    1
run_test "extra succneqq"           '\succneqq'                       1
run_test "extra succnsim"           '\succnsim'                       1
run_test "extra succsim"            '\succsim'                        1
run_test "extra sum"                '\sum'                            2000
run_test "extra sup"                '\sup'                            5000
run_test "extra supset"             '\supset'                         5000
run_test "extra Supset"             '\Supset'                         1
run_test "extra supseteq"           '\supseteq'                       5000
run_test "extra supseteqq"          '\supseteqq'                      1
run_test "extra supsetneq"          '\supsetneq'                      5000
run_test "extra supsetneqq"         '\supsetneqq'                     1
run_test "extra surd"               '\surd'                           2000
run_test "extra swarrow"            '\swarrow'                        2000
run_test "extra tan"                '\tan'                            5000
run_test "extra tanh"               '\tanh'                           5000
run_test "extra tau"                '\tau'                            2000
run_test "extra text"               '\text{hello}'                    3000
run_test "extra textrm"             '\textrm{text}'                   1
run_test "extra textsf"             '\textsf{text}'                   1
run_test "extra texttt"             '\texttt{text}'                   1
run_test "extra textbf"             '\textbf{bold}'                   1
run_test "extra textit"             '\textit{italic}'                 3000
run_test "extra textsc"             '\textsc{text}'                   1
run_test "extra textsl"             '\textsl{text}'                   1
run_test "extra textup"             '\textup{text}'                   3000
run_test "extra textmd"             '\textmd{text}'                   1
run_test "extra textnormal"         '\textnormal{text}'               3000
run_test "extra therefore"          '\therefore'                      5000
run_test "extra theta"              '\theta'                          2000
run_test "extra Theta"              '\Theta'                          2000
run_test "extra thickapprox"        '\thickapprox'                    1
run_test "extra thicksim"           '\thicksim'                       1
run_test "extra thinspace"          '\thinspace'                      1
run_test "extra times"              '\times'                          5000
run_test "extra to"                 '\to'                             5000
run_test "extra top"                '\top'                            2000
run_test "extra triangle"           '\triangle'                       2000
run_test "extra triangledown"       '\triangledown'                   1
run_test "extra triangleleft"       '\triangleleft'                   5000
run_test "extra trianglelefteq"     '\trianglelefteq'                 5000
run_test "extra triangleq"          '\triangleq'                      1
run_test "extra triangleright"      '\triangleright'                  5000
run_test "extra trianglerighteq"    '\trianglerighteq'                5000
run_test "extra tt"                 '{\tt x}'                         1
run_test "extra twoheadleftarrow"   '\twoheadleftarrow'               3000
run_test "extra twoheadrightarrow"  '\twoheadrightarrow'              3000
run_test "extra ulcorner"           '\ulcorner'                       2000
run_test "extra underbrace"         '\underbrace{x+y}'                10000
run_test "extra underbracket"       '\underbracket{abc}'              1
run_test "extra underleftarrow"     '\underleftarrow{abc}'            1
run_test "extra underleftrightarrow"'\underleftrightarrow{abc}'       1
run_test "extra underline"          '\underline{x}'                   5000
run_test "extra underparen"         '\underparen{abc}'                1
run_test "extra underrightarrow"    '\underrightarrow{abc}'           1
run_test "extra underset"           '\underset{x\in X}{\max}'         10000
run_test "extra unlhd"              '\unlhd'                          1
run_test "extra unrhd"              '\unrhd'                          1
run_test "extra uparrow"            '\uparrow'                        2000
run_test "extra Uparrow"            '\Uparrow'                        2000
run_test "extra updownarrow"        '\updownarrow'                    3000
run_test "extra Updownarrow"        '\Updownarrow'                    3000
run_test "extra upharpoonleft"      '\upharpoonleft'                  1
run_test "extra upharpoonright"     '\upharpoonright'                 1
run_test "extra uplus"              '\uplus'                          5000
run_test "extra upuparrows"         '\upuparrows'                     1
run_test "extra urcorner"           '\urcorner'                       2000
run_test "extra varepsilon"         '\varepsilon'                     2000
run_test "extra varGamma"           '\varGamma'                       2000
run_test "extra varinjlim"          '\varinjlim'                      12000
run_test "extra varkappa"           '\varkappa'                       1
run_test "extra varLambda"          '\varLambda'                      2000
run_test "extra varliminf"          '\varliminf'                      12000
run_test "extra varlimsup"          '\varlimsup'                      12000
run_test "extra varnothing"         '\varnothing'                     2000
run_test "extra varOmega"           '\varOmega'                       2000
run_test "extra varphi"             '\varphi'                         2000
run_test "extra varPhi"             '\varPhi'                         2000
run_test "extra varpi"              '\varpi'                          2000
run_test "extra varPi"              '\varPi'                          2000
run_test "extra varprojlim"         '\varprojlim'                     12000
run_test "extra varpropto"          '\varpropto'                      1
run_test "extra varPsi"             '\varPsi'                         2000
run_test "extra varrho"             '\varrho'                         2000
run_test "extra varsigma"           '\varsigma'                       2000
run_test "extra varSigma"           '\varSigma'                       2000
run_test "extra varsubsetneq"       '\varsubsetneq'                   1
run_test "extra varsubsetneqq"      '\varsubsetneqq'                  1
run_test "extra varsupsetneq"       '\varsupsetneq'                   1
run_test "extra varsupsetneqq"      '\varsupsetneqq'                  1
run_test "extra vartheta"           '\vartheta'                       2000
run_test "extra varTheta"           '\varTheta'                       2000
run_test "extra vartriangle"        '\vartriangle'                    5000
run_test "extra vartriangleleft"    '\vartriangleleft'                5000
run_test "extra vartriangleright"   '\vartriangleright'               5000
run_test "extra varUpsilon"         '\varUpsilon'                     1
run_test "extra varXi"              '\varXi'                          2000
run_test "extra vdash"              '\vdash'                          5000
run_test "extra vDash"              '\vDash'                          5000
run_test "extra Vdash"              '\Vdash'                          5000
run_test "extra vdots"              '\vdots'                          2000
run_test "extra vec"                '\vec{x}'                         3000
run_test "extra vee"                '\vee'                            5000
run_test "extra veebar"             '\veebar'                         5000
run_test "extra vert"               '\vert'                           2000
run_test "extra Vert"               '\Vert'                           2000
run_test "extra vphantom"           '\vphantom{x}'                    1
run_test "extra Vvdash"             '\Vvdash'                         5000
run_test "extra wedge"              '\wedge'                          5000
run_test "extra widehat"            '\widehat{xy}'                    5000
run_test "extra widetilde"          '\widetilde{xy}'                  5000
run_test "extra wp"                 '\wp'                             2000
run_test "extra wr"                 '\wr'                             2000
run_test "extra xcancel"            '\xcancel{x}'                     5000
run_test "extra xi"                 '\xi'                             2000
run_test "extra Xi"                 '\Xi'                             2000
run_test "extra xleftarrow"         '\xleftarrow{abc}'                15000
run_test "extra xrightarrow"        '\xrightarrow{abc}'               15000
run_test "extra xRightarrow"        '\xRightarrow{abc}'               15000
run_test "extra xLeftarrow"         '\xLeftarrow{abc}'                15000
run_test "extra xLeftrightarrow"    '\xLeftrightarrow{abc}'           15000
run_test "extra xleftrightarrow"    '\xleftrightarrow{abc}'           15000
run_test "extra xmapsto"            '\xmapsto{abc}'                   15000
run_test "extra xhookleftarrow"     '\xhookleftarrow{abc}'            15000
run_test "extra xhookrightarrow"    '\xhookrightarrow{abc}'           15000
run_test "extra xrightharpoondown"  '\xrightharpoondown{abc}'        10000
run_test "extra xrightharpoonup"    '\xrightharpoonup{abc}'          12000
run_test "extra xleftharpoondown"   '\xleftharpoondown{abc}'         10000
run_test "extra xleftharpoonup"     '\xleftharpoonup{abc}'           12000
run_test "extra xrightleftharpoons"  '\xrightleftharpoons{abc}'      1
run_test "extra xleftrightharpoons"  '\xleftrightharpoons{abc}'      1
run_test "extra xtwoheadleftarrow"  '\xtwoheadleftarrow{abc}'        1
run_test "extra xtwoheadrightarrow" '\xtwoheadrightarrow{abc}'       1
run_test "extra yen"                '\yen'                            1
run_test "extra zeta"               '\zeta'                           2000

# =====================================================================
# Summary
# =====================================================================
echo ""
echo "================================================"
echo "  TEST SUMMARY"
echo "================================================"
echo "PASS: $PASS"
echo "FAIL: $FAIL"
echo "ERRORS: $ERRORS"
if [ "$ERRORS" -gt 0 ]; then
  echo "SOME TESTS FAILED - review above for details"
  exit 1
else
  echo "ALL TESTS PASSED!"
  exit 0
fi
