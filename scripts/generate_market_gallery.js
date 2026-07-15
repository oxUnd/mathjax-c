#!/usr/bin/env node

const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');

const root = path.resolve(__dirname, '..');
const testFile = path.join(root, 'tools', 'test.sh');
const outDir = path.join(root, 'test', 'diff');
const assetDir = path.join(outDir, 'assets');
const cli = process.env.CLI || path.join(root, 'build', 'mathjax-cli');
const font = process.env.FONT || path.join(root, 'fonts', 'STIXTwoMath-Regular.ttf');
const concurrency = Math.max(1, Math.min(Number(process.env.JOBS || os.cpus().length || 4), 12));

function htmlEscape(s) {
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function parseTestCases() {
  const lines = fs.readFileSync(testFile, 'utf8').split(/\r?\n/);
  const cases = [];
  let section = 'Test Suite';
  for (const line of lines) {
    const sectionMatch = line.match(/^echo "===\s*(.*?)\s*==="/);
    if (sectionMatch) {
      section = `Test Suite / ${sectionMatch[1]}`;
      continue;
    }
    const testMatch = line.match(/^run_test\s+"([^"]+)"\s*'([^']*)'\s+([0-9]+)/);
    if (!testMatch) continue;
    cases.push({
      section,
      name: testMatch[1],
      formula: testMatch[2],
      source: 'tools/test.sh',
      lenient: Number(testMatch[3]) === 1,
    });
  }
  return cases;
}

const marketCases = [
  ['Algebra', 'binomial theorem', '(x+y)^n=\\sum_{k=0}^{n}\\binom{n}{k}x^{n-k}y^k'],
  ['Algebra', 'quadratic formula', 'x=\\frac{-b\\pm\\sqrt{b^2-4ac}}{2a}'],
  ['Algebra', 'difference of squares', 'a^2-b^2=(a-b)(a+b)'],
  ['Algebra', 'geometric series finite', '\\sum_{i=0}^{n} ar^i=a\\frac{1-r^{n+1}}{1-r}'],
  ['Algebra', 'geometric series infinite', '\\sum_{i=0}^{\\infty} ar^i=\\frac{a}{1-r},\\quad |r|<1'],
  ['Algebra', 'partial fractions', '\\frac{1}{x^2-1}=\\frac{1}{2}\\left(\\frac{1}{x-1}-\\frac{1}{x+1}\\right)'],
  ['Algebra', 'continued fraction', 'x=a_0+\\cfrac{1}{a_1+\\cfrac{1}{a_2+\\cfrac{1}{a_3}}}'],
  ['Algebra', 'old tex over', '{a+b \\over c+d}'],
  ['Algebra', 'old tex choose', '{n \\choose k}={n!\\over k!(n-k)!}'],
  ['Algebra', 'old tex brack', '{n \\brack k}_q'],
  ['Algebra', 'old tex brace', '{n \\brace k}'],

  ['Calculus', 'limit definition', '\\lim_{x\\to a} f(x)=L'],
  ['Calculus', 'epsilon delta', '\\forall\\epsilon>0\\;\\exists\\delta>0:\\;0<|x-a|<\\delta\\Rightarrow |f(x)-L|<\\epsilon'],
  ['Calculus', 'derivative definition', 'f\\prime(a)=\\lim_{h\\to0}\\frac{f(a+h)-f(a)}{h}'],
  ['Calculus', 'second derivative', '\\frac{d^2y}{dx^2}+p(x)\\frac{dy}{dx}+q(x)y=0'],
  ['Calculus', 'chain rule', '\\frac{d}{dx}f(g(x))=f\\prime(g(x))g\\prime(x)'],
  ['Calculus', 'product rule', '(fg)\\prime=f\\prime g+fg\\prime'],
  ['Calculus', 'integration by parts', '\\int u\\,dv=uv-\\int v\\,du'],
  ['Calculus', 'substitution', '\\int_a^b f(g(x))g\\prime(x)\\,dx=\\int_{g(a)}^{g(b)}f(u)\\,du'],
  ['Calculus', 'fundamental theorem', '\\int_a^b f\\prime(x)\\,dx=f(b)-f(a)'],
  ['Calculus', 'multiple integral', '\\iiint_V \\rho(x,y,z)\\,dV'],
  ['Calculus', 'surface integral', '\\iint_S \\mathbf F\\cdot\\mathbf n\\,dS'],
  ['Calculus', 'closed contour', '\\oint_C \\mathbf F\\cdot d\\mathbf r'],
  ['Calculus', 'gradient', '\\nabla f=\\left(\\frac{\\partial f}{\\partial x},\\frac{\\partial f}{\\partial y},\\frac{\\partial f}{\\partial z}\\right)'],
  ['Calculus', 'laplacian', '\\Delta f=\\nabla^2f=\\frac{\\partial^2 f}{\\partial x^2}+\\frac{\\partial^2 f}{\\partial y^2}'],
  ['Calculus', 'taylor series', 'f(x)=\\sum_{n=0}^{\\infty}\\frac{f^{(n)}(a)}{n!}(x-a)^n'],
  ['Calculus', 'maclaurin exp', 'e^x=\\sum_{n=0}^{\\infty}\\frac{x^n}{n!}'],
  ['Calculus', 'fourier series', 'f(x)\\sim\\frac{a_0}{2}+\\sum_{n=1}^{\\infty}(a_n\\cos nx+b_n\\sin nx)'],

  ['Linear Algebra', 'matrix product', '\\begin{pmatrix}a&b\\\\c&d\\end{pmatrix}\\begin{pmatrix}x\\\\y\\end{pmatrix}=\\begin{pmatrix}ax+by\\\\cx+dy\\end{pmatrix}'],
  ['Linear Algebra', 'det 2x2', '\\det\\begin{pmatrix}a&b\\\\c&d\\end{pmatrix}=ad-bc'],
  ['Linear Algebra', 'inverse 2x2', 'A^{-1}=\\frac{1}{ad-bc}\\begin{pmatrix}d&-b\\\\-c&a\\end{pmatrix}'],
  ['Linear Algebra', 'eigen equation', 'A\\mathbf v=\\lambda\\mathbf v'],
  ['Linear Algebra', 'characteristic polynomial', 'p_A(\\lambda)=\\det(\\lambda I-A)'],
  ['Linear Algebra', 'trace determinant', '\\operatorname{tr}(AB)=\\operatorname{tr}(BA)'],
  ['Linear Algebra', 'rank nullity', '\\dim V=\\operatorname{rank}T+\\operatorname{nullity}T'],
  ['Linear Algebra', 'orthogonal projection', '\\operatorname{proj}_{\\mathbf u}\\mathbf v=\\frac{\\mathbf v\\cdot\\mathbf u}{\\mathbf u\\cdot\\mathbf u}\\mathbf u'],
  ['Linear Algebra', 'svd', 'A=U\\Sigma V^T'],
  ['Linear Algebra', 'block matrix', '\\begin{bmatrix}A&B\\\\C&D\\end{bmatrix}'],
  ['Linear Algebra', 'cases matrix', '\\left[\\begin{array}{cc|c}1&0&a\\\\0&1&b\\end{array}\\right]'],

  ['Probability', 'bayes theorem', 'P(A\\mid B)=\\frac{P(B\\mid A)P(A)}{P(B)}'],
  ['Probability', 'total probability', 'P(A)=\\sum_i P(A\\mid B_i)P(B_i)'],
  ['Probability', 'expectation discrete', '\\mathbb E[X]=\\sum_x x\\,P(X=x)'],
  ['Probability', 'expectation continuous', '\\mathbb E[X]=\\int_{-\\infty}^{\\infty}x f_X(x)\\,dx'],
  ['Probability', 'variance', '\\operatorname{Var}(X)=\\mathbb E[X^2]-\\mathbb E[X]^2'],
  ['Probability', 'normal distribution', 'f(x)=\\frac{1}{\\sigma\\sqrt{2\\pi}}e^{-\\frac{1}{2}\\left(\\frac{x-\\mu}{\\sigma}\\right)^2}'],
  ['Probability', 'binomial distribution', 'P(X=k)=\\binom{n}{k}p^k(1-p)^{n-k}'],
  ['Probability', 'poisson distribution', 'P(X=k)=e^{-\\lambda}\\frac{\\lambda^k}{k!}'],
  ['Probability', 'law large numbers', '\\overline X_n\\xrightarrow{p}\\mu'],
  ['Probability', 'central limit theorem', '\\frac{\\sqrt n(\\overline X_n-\\mu)}{\\sigma}\\xrightarrow{d}N(0,1)'],

  ['Statistics', 'sample mean', '\\bar{x}=\\frac{1}{n}\\sum_{i=1}^{n}x_i'],
  ['Statistics', 'sample variance', 's^2=\\frac{1}{n-1}\\sum_{i=1}^{n}(x_i-\\bar{x})^2'],
  ['Statistics', 'least squares', '\\hat\\beta=(X^TX)^{-1}X^Ty'],
  ['Statistics', 'log likelihood', '\\ell(\\theta)=\\sum_{i=1}^{n}\\log f(x_i;\\theta)'],
  ['Statistics', 'confidence interval', '\\bar X\\pm z_{\\alpha/2}\\frac{\\sigma}{\\sqrt n}'],
  ['Statistics', 't statistic', 't=\\frac{\\bar X-\\mu_0}{s/\\sqrt n}'],
  ['Statistics', 'chi square', '\\chi^2=\\sum_{i=1}^{k}\\frac{(O_i-E_i)^2}{E_i}'],
  ['Statistics', 'correlation', 'r=\\frac{\\sum_i(x_i-\\bar x)(y_i-\\bar y)}{\\sqrt{\\sum_i(x_i-\\bar x)^2\\sum_i(y_i-\\bar y)^2}}'],

  ['Discrete Math', 'set builder', 'A=\\{x\\in\\mathbb R\\mid x^2<2\\}'],
  ['Discrete Math', 'de morgan', '\\overline{A\\cup B}=\\overline A\\cap\\overline B'],
  ['Discrete Math', 'logic equivalence', '(p\\Rightarrow q)\\Leftrightarrow(\\neg q\\Rightarrow\\neg p)'],
  ['Discrete Math', 'inclusion exclusion', '|A\\cup B\\cup C|=|A|+|B|+|C|-|A\\cap B|-|A\\cap C|-|B\\cap C|+|A\\cap B\\cap C|'],
  ['Discrete Math', 'graph degree sum', '\\sum_{v\\in V}\\deg(v)=2|E|'],
  ['Discrete Math', 'recurrence', 'T(n)=2T(n/2)+n'],
  ['Discrete Math', 'big o', 'f(n)=O(g(n))\\iff \\exists C,n_0\\;\\forall n\\ge n_0:\\;f(n)\\le Cg(n)'],
  ['Discrete Math', 'stirling', 'n!\\sim\\sqrt{2\\pi n}\\left(\\frac{n}{e}\\right)^n'],

  ['Number Theory', 'euclid gcd', '\\gcd(a,b)=\\gcd(b,a\\bmod b)'],
  ['Number Theory', 'bezout', 'ax+by=\\gcd(a,b)'],
  ['Number Theory', 'fermat little theorem', 'a^{p-1}\\equiv1\\pmod p'],
  ['Number Theory', 'euler theorem', 'a^{\\varphi(n)}\\equiv1\\pmod n'],
  ['Number Theory', 'chinese remainder', 'x\\equiv a_i\\pmod{m_i},\\quad i=1,\\dots,k'],
  ['Number Theory', 'prime zeta style', '\\sum_{p\\ \\mathrm{prime}}\\frac{1}{p^s}'],
  ['Number Theory', 'riemann zeta', '\\zeta(s)=\\sum_{n=1}^{\\infty}\\frac{1}{n^s}=\\prod_p\\frac{1}{1-p^{-s}}'],

  ['Physics', 'newton second law', '\\mathbf F=m\\mathbf a'],
  ['Physics', 'lagrangian', '\\frac{d}{dt}\\frac{\\partial L}{\\partial \\dot q_i}-\\frac{\\partial L}{\\partial q_i}=0'],
  ['Physics', 'hamilton equations', '\\dot q_i=\\frac{\\partial H}{\\partial p_i},\\quad \\dot p_i=-\\frac{\\partial H}{\\partial q_i}'],
  ['Physics', 'maxwell equations', '\\nabla\\cdot\\mathbf E=\\frac{\\rho}{\\varepsilon_0},\\quad \\nabla\\times\\mathbf B=\\mu_0\\mathbf J+\\mu_0\\varepsilon_0\\frac{\\partial\\mathbf E}{\\partial t}'],
  ['Physics', 'schrodinger', 'i\\hbar\\frac{\\partial}{\\partial t}\\Psi=\\hat H\\Psi'],
  ['Physics', 'heisenberg uncertainty', '\\Delta x\\Delta p\\ge\\frac{\\hbar}{2}'],
  ['Physics', 'einstein field', 'G_{\\mu\\nu}+\\Lambda g_{\\mu\\nu}=\\frac{8\\pi G}{c^4}T_{\\mu\\nu}'],
  ['Physics', 'dirac notation', '\\langle\\psi|\\hat A|\\phi\\rangle'],
  ['Physics', 'fourier transform', '\\hat f(\\xi)=\\int_{-\\infty}^{\\infty}f(x)e^{-2\\pi i x\\xi}\\,dx'],

  ['Optimization', 'argmin', 'x^*=\\operatorname*{arg\\,min}_{x\\in\\mathcal X} f(x)'],
  ['Optimization', 'lagrange multipliers', '\\nabla f(x)=\\lambda\\nabla g(x)'],
  ['Optimization', 'kkt', '\\nabla f(x^*)+\\sum_i\\lambda_i\\nabla g_i(x^*)=0,\\quad \\lambda_i g_i(x^*)=0'],
  ['Optimization', 'gradient descent', 'x_{t+1}=x_t-\\eta\\nabla f(x_t)'],
  ['Optimization', 'newton method', 'x_{t+1}=x_t-H_f(x_t)^{-1}\\nabla f(x_t)'],
  ['Optimization', 'convexity', 'f(\\theta x+(1-\\theta)y)\\le\\theta f(x)+(1-\\theta)f(y)'],
  ['Optimization', 'dual problem', 'g(\\lambda,\\nu)=\\inf_x L(x,\\lambda,\\nu)'],

  ['Machine Learning', 'softmax', '\\operatorname{softmax}(z_i)=\\frac{e^{z_i}}{\\sum_j e^{z_j}}'],
  ['Machine Learning', 'cross entropy', 'H(p,q)=-\\sum_x p(x)\\log q(x)'],
  ['Machine Learning', 'mse', 'L(\\theta)=\\frac{1}{n}\\sum_{i=1}^{n}(y_i-f_\\theta(x_i))^2'],
  ['Machine Learning', 'logistic regression', 'P(y=1\\mid x)=\\sigma(w^Tx+b)=\\frac{1}{1+e^{-(w^Tx+b)}}'],
  ['Machine Learning', 'attention', '\\operatorname{Attention}(Q,K,V)=\\operatorname{softmax}\\left(\\frac{QK^T}{\\sqrt{d_k}}\\right)V'],
  ['Machine Learning', 'batch norm', '\\hat x_i=\\frac{x_i-\\mu_B}{\\sqrt{\\sigma_B^2+\\epsilon}}'],
  ['Machine Learning', 'adam moment', 'm_t=\\beta_1m_{t-1}+(1-\\beta_1)g_t'],
  ['Machine Learning', 'vae elbo', '\\mathcal L(\\theta,\\phi;x)=\\mathbb E_{q_\\phi(z\\mid x)}[\\log p_\\theta(x\\mid z)]-D_{KL}(q_\\phi(z\\mid x)\\|p(z))'],

  ['Geometry', 'distance formula', 'd(p,q)=\\sqrt{(x_2-x_1)^2+(y_2-y_1)^2}'],
  ['Geometry', 'law of cosines', 'c^2=a^2+b^2-2ab\\cos C'],
  ['Geometry', 'area triangle', 'K=\\frac{1}{2}ab\\sin C'],
  ['Geometry', 'circle area', 'A=\\pi r^2'],
  ['Geometry', 'curvature', '\\kappa=\\frac{|x\\prime y\\prime\\prime-y\\prime x\\prime\\prime|}{((x\\prime)^2+(y\\prime)^2)^{3/2}}'],
  ['Geometry', 'metric tensor', 'ds^2=g_{ij}dx^i dx^j'],

  ['Special Functions', 'gamma function', '\\Gamma(z)=\\int_0^\\infty t^{z-1}e^{-t}\\,dt'],
  ['Special Functions', 'beta function', 'B(x,y)=\\frac{\\Gamma(x)\\Gamma(y)}{\\Gamma(x+y)}'],
  ['Special Functions', 'bessel equation', 'x^2y\\prime\\prime+xy\\prime+(x^2-\\nu^2)y=0'],
  ['Special Functions', 'legendre equation', '(1-x^2)y\\prime\\prime-2xy\\prime+\\ell(\\ell+1)y=0'],
  ['Special Functions', 'erf', '\\operatorname{erf}(x)=\\frac{2}{\\sqrt\\pi}\\int_0^x e^{-t^2}\\,dt'],

  ['AMS Environments', 'cases piecewise', 'f(x)=\\begin{cases}x^2,&x\\ge0\\\\-x,&x<0\\end{cases}'],
  ['AMS Environments', 'aligned equations', '\\begin{aligned}a&=b+c\\\\d&=e+f\\end{aligned}'],
  ['AMS Environments', 'split equation', '\\begin{split}(a+b)^2&=a^2+2ab+b^2\\\\&=b^2+2ab+a^2\\end{split}'],
  ['AMS Environments', 'array with alignment', '\\begin{array}{rcl}x&=&1\\\\y&=&2\\end{array}'],
  ['AMS Environments', 'smallmatrix inline', '\\left(\\begin{smallmatrix}a&b\\\\c&d\\end{smallmatrix}\\right)'],
  ['AMS Environments', 'vmatrix determinant', '\\begin{vmatrix}a&b\\\\c&d\\end{vmatrix}'],
  ['AMS Environments', 'Vmatrix norm', '\\begin{Vmatrix}x\\\\y\\end{Vmatrix}'],

  ['Multiline Complex', 'three line aligned calculus', '\\begin{aligned}F(x)&=\\int_0^x e^{-t^2}\\,dt\\\\F\\prime(x)&=e^{-x^2}\\\\F\\prime\\prime(x)&=-2xe^{-x^2}\\end{aligned}'],
  ['Multiline Complex', 'three line piecewise recurrence', 'a_n=\\begin{cases}1,&n=0\\\\a_{n-1}+2n-1,&n>0\\\\0,&n<0\\end{cases}'],
  ['Multiline Complex', 'three line optimization system', '\\begin{aligned}\\nabla f(x^*)+A^T\\lambda+B^T\\mu&=0\\\\Ax^*-b&=0\\\\\\mu_i(Bx^*-c)_i&=0\\end{aligned}'],
  ['Multiline Complex', 'four line maxwell system', '\\begin{aligned}\\nabla\\cdot\\mathbf E&=\\frac{\\rho}{\\varepsilon_0}\\\\\\nabla\\cdot\\mathbf B&=0\\\\\\nabla\\times\\mathbf E&=-\\frac{\\partial\\mathbf B}{\\partial t}\\\\\\nabla\\times\\mathbf B&=\\mu_0\\mathbf J+\\mu_0\\varepsilon_0\\frac{\\partial\\mathbf E}{\\partial t}\\end{aligned}'],
  ['Multiline Complex', 'four line matrix transform', '\\begin{pmatrix}x\\prime\\\\y\\prime\\\\z\\prime\\\\1\\end{pmatrix}=\\begin{pmatrix}a_{11}&a_{12}&a_{13}&t_x\\\\a_{21}&a_{22}&a_{23}&t_y\\\\a_{31}&a_{32}&a_{33}&t_z\\\\0&0&0&1\\end{pmatrix}\\begin{pmatrix}x\\\\y\\\\z\\\\1\\end{pmatrix}'],
  ['Multiline Complex', 'four line bayes table', '\\begin{array}{rcl}P(A\\mid B)&=&\\dfrac{P(B\\mid A)P(A)}{P(B)}\\\\P(B)&=&\\sum_i P(B\\mid A_i)P(A_i)\\\\\\mathbb E[X]&=&\\sum_x xP(X=x)\\\\\\operatorname{Var}(X)&=&\\mathbb E[X^2]-\\mathbb E[X]^2\\end{array}'],
  ['Multiline Complex', 'four line nested scripts', '\\begin{aligned}S_0&=\\sum_{i=1}^{n}x_i\\\\S_1&=\\sum_{i=1}^{n}i\\,x_i^{2}\\\\S_2&=\\sum_{i=1}^{n}\\frac{x_i^{i+1}}{1+x_i^2}\\\\T&=\\sqrt{S_0^2+S_1^2+S_2^2}\\end{aligned}'],

  ['Accents And Decorations', 'widehat vector', '\\widehat{abcdef}'],
  ['Accents And Decorations', 'widetilde vector', '\\widetilde{abcdef}'],
  ['Accents And Decorations', 'overline long', '\\overline{x_1+x_2+\\cdots+x_n}'],
  ['Accents And Decorations', 'underbrace sum', '\\underbrace{1+1+\\cdots+1}_{n\\text{ times}}=n'],
  ['Accents And Decorations', 'overbrace sum', '\\overbrace{x+x+\\cdots+x}^{n\\text{ times}}=nx'],
  ['Accents And Decorations', 'vec grad', '\\vec v=\\langle v_1,v_2,v_3\\rangle'],
  ['Accents And Decorations', 'dot double dot', '\\dot x^2+\\ddot x'],
  ['Accents And Decorations', 'cancel expression', '\\cancel{x}+\\bcancel{y}+\\xcancel{z}'],

  ['Arrows And Relations', 'xrightarrow labels', 'A\\xrightarrow[below]{above}B'],
  ['Arrows And Relations', 'xtwohead labels', 'A\\xtwoheadrightarrow[def]{abc}B'],
  ['Arrows And Relations', 'harpoon labels', 'A\\xrightharpoonup[below]{above}B'],
  ['Arrows And Relations', 'long exact sequence', '0\\to A\\to B\\to C\\to0'],
  ['Arrows And Relations', 'commutative square', '\\begin{matrix}A&\\xrightarrow{f}&B\\\\\\downarrow&&\\downarrow\\\\C&\\xrightarrow{g}&D\\end{matrix}'],
  ['Arrows And Relations', 'relations chain', 'a\\le b\\le c\\implies a\\le c'],
  ['Arrows And Relations', 'negated relations', 'a\\nleq b,\\quad A\\nsubseteq B'],

  ['Delimiters', 'left right norm', '\\left\\|\\frac{x}{y}\\right\\|'],
  ['Delimiters', 'floor ceil', '\\left\\lfloor\\frac{n}{2}\\right\\rfloor+\\left\\lceil\\frac{n}{2}\\right\\rceil=n'],
  ['Delimiters', 'angle brackets', '\\left\\langle x,y\\right\\rangle=\\sum_i x_i y_i'],
  ['Delimiters', 'middle separator', '\\left\\{x\\in X\\middle|x>0\\right\\}'],
  ['Delimiters', 'big delimiters', '\\Biggl(\\sum_{i=1}^{n}a_i\\Biggr)^2'],

  ['Fonts And Styles', 'mathcal', '\\mathcal F\\{f(t)\\}=F(\\omega)'],
  ['Fonts And Styles', 'mathbb', '\\mathbb N\\subset\\mathbb Z\\subset\\mathbb Q\\subset\\mathbb R\\subset\\mathbb C'],
  ['Fonts And Styles', 'mathfrak', '\\mathfrak g=\\mathfrak{sl}_2(\\mathbb C)'],
  ['Fonts And Styles', 'bold vectors', '\\mathbf x^T\\mathbf y=\\sum_i x_i y_i'],
  ['Fonts And Styles', 'text in math', '\\text{if }x>0\\text{ then }|x|=x'],

  ['Edge Syntax Variants', 'unicode greek input', 'α+β=γ'],
  ['Edge Syntax Variants', 'escaped braces', '\\{a,b,c\\}'],
  ['Edge Syntax Variants', 'spaces commands', 'x\\,y\\;z\\quad w'],
  ['Edge Syntax Variants', 'substack', '\\sum_{\\substack{1\\le i\\le n\\\\i\\text{ odd}}}i^2'],
  ['Edge Syntax Variants', 'limits explicit', '\\int\\limits_0^1 x^2\\,dx'],
  ['Edge Syntax Variants', 'nolimits explicit', '\\sum\\nolimits_{i=1}^{n}a_i'],
  ['Edge Syntax Variants', 'root tex primitive', '\\root 3 \\of {x+1}'],
  ['Edge Syntax Variants', 'nested radicals', '\\sqrt{1+\\sqrt{1+\\sqrt{1+x}}}'],
  ['Edge Syntax Variants', 'complex scripts', 'x_{i_1,i_2}^{j_1,j_2}'],
  ['Edge Syntax Variants', 'prescripts', '\\prescript{14}{6}{\\mathbf C}'],
];

function makeMarketCases() {
  return marketCases.map(([section, name, formula]) => ({
    section: `Market Corpus / ${section}`,
    name,
    formula,
    source: 'curated common formulas',
    lenient: false,
  }));
}

function assignIds(cases) {
  return cases.map((item, idx) => ({
    ...item,
    index: idx + 1,
    id: String(idx + 1).padStart(4, '0'),
    image: `assets/${String(idx + 1).padStart(4, '0')}.png`,
  }));
}

function buildCases() {
  const seen = new Set();
  const all = [];
  for (const item of [...parseTestCases(), ...makeMarketCases()]) {
    const key = `${item.section}\n${item.name}\n${item.formula}`;
    if (seen.has(key)) continue;
    seen.add(key);
    all.push(item);
  }
  return assignIds(all);
}

function renderCase(testCase) {
  return new Promise((resolve) => {
    const imagePath = path.join(assetDir, `${testCase.id}.png`);
    const child = spawn(cli, ['-f', font, '-o', imagePath, testCase.formula], {
      cwd: root,
      stdio: ['ignore', 'pipe', 'pipe'],
    });
    let stdout = '';
    let stderr = '';
    child.stdout.on('data', (chunk) => { stdout += chunk; });
    child.stderr.on('data', (chunk) => { stderr += chunk; });
    child.on('close', (code) => {
      const sizeMatch = `${stdout}\n${stderr}`.match(/Wrote\s+([0-9]+)x([0-9]+)\s+PNG/);
      resolve({
        ...testCase,
        ok: code === 0 && fs.existsSync(imagePath),
        width: sizeMatch ? Number(sizeMatch[1]) : null,
        height: sizeMatch ? Number(sizeMatch[2]) : null,
        stdout: stdout.trim(),
        stderr: stderr.trim(),
      });
    });
  });
}

async function renderAll(cases) {
  const results = new Array(cases.length);
  let next = 0;
  let done = 0;
  async function worker() {
    while (next < cases.length) {
      const current = next++;
      results[current] = await renderCase(cases[current]);
      done++;
      if (done % 50 === 0 || done === cases.length) {
        process.stdout.write(`Rendered ${done}/${cases.length}\n`);
      }
    }
  }
  await Promise.all(Array.from({ length: concurrency }, worker));
  return results;
}

function writeHtml(results) {
  const sections = [...new Set(results.map((item) => item.section))];
  const failures = results.filter((item) => !item.ok);
  const lenient = results.filter((item) => item.lenient).length;
  const generatedAt = new Date().toISOString();
  const cards = results.map((item) => `
      <article class="case${item.ok ? '' : ' failed'}" data-section="${htmlEscape(item.section)}">
        <header>
          <span class="idx">#${item.id}</span>
          <h2>${htmlEscape(item.name)}</h2>
          <span class="tag">${htmlEscape(item.section)}</span>
          <span class="tag">${htmlEscape(item.source || 'corpus')}</span>
          ${item.lenient ? '<span class="tag">lenient</span>' : ''}
          ${item.ok ? '' : '<span class="tag bad">render failed</span>'}
        </header>
        <p class="source"><code>${htmlEscape(item.formula || '(empty)')}</code></p>
        <div class="compare">
          <section>
            <h3>mathjax-c ${item.width && item.height ? `<small>${item.width}x${item.height}</small>` : ''}</h3>
            <div class="render c-render">${item.ok ? `<img loading="lazy" src="${item.image}" alt="${htmlEscape(item.name)} by mathjax-c">` : `<pre>${htmlEscape(item.stderr || item.stdout || 'failed')}</pre>`}</div>
          </section>
          <section>
            <h3>MathJax reference</h3>
            <div class="render mjx-render">$$${htmlEscape(item.formula)}$$</div>
          </section>
        </div>
      </article>`).join('\n');

  const html = `<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>mathjax-c 市面常见 LaTeX 数学公式对照</title>
  <style>
    :root {
      color-scheme: light;
      --bg: #f4f6f9;
      --panel: #ffffff;
      --ink: #111827;
      --muted: #5f6b7a;
      --line: #d8dee8;
      --soft: #eef2f7;
      --accent: #185abc;
      --bad: #b42318;
    }
    * { box-sizing: border-box; }
    body { margin: 0; background: var(--bg); color: var(--ink); font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; }
    .top { position: sticky; top: 0; z-index: 10; border-bottom: 1px solid var(--line); background: rgba(244,246,249,.96); backdrop-filter: blur(10px); }
    .top-inner, main { width: min(1500px, calc(100vw - 28px)); margin: 0 auto; }
    .top-inner { padding: 18px 0 14px; }
    h1 { margin: 0 0 10px; font-size: 24px; line-height: 1.2; }
    .summary { display: flex; flex-wrap: wrap; gap: 8px; color: var(--muted); font-size: 13px; }
    .pill { border: 1px solid var(--line); border-radius: 999px; padding: 6px 10px; background: var(--panel); }
    .controls { display: grid; grid-template-columns: minmax(220px, 1fr) minmax(220px, 360px); gap: 10px; margin-top: 14px; }
    input, select { height: 36px; border: 1px solid var(--line); border-radius: 6px; background: #fff; color: var(--ink); font: inherit; padding: 0 10px; }
    main { padding: 18px 0 44px; }
    .case { margin-bottom: 14px; border: 1px solid var(--line); border-radius: 8px; background: var(--panel); overflow: hidden; }
    .case.failed { border-color: #f0aaa3; }
    .case header { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; padding: 12px 14px; border-bottom: 1px solid var(--line); background: #fbfcfe; }
    .idx, .tag { flex: none; border-radius: 999px; padding: 4px 8px; background: var(--soft); color: var(--muted); font-size: 12px; }
    .tag.bad { color: var(--bad); background: #fee4e2; }
    h2 { margin: 0; font-size: 15px; line-height: 1.35; }
    .source { margin: 0; padding: 10px 14px 0; color: var(--muted); font-size: 13px; word-break: break-word; }
    code { font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace; }
    .compare { display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 1fr); gap: 12px; padding: 12px 14px 14px; }
    h3 { margin: 0 0 8px; color: var(--accent); font-size: 12px; letter-spacing: .06em; text-transform: uppercase; }
    h3 small { color: var(--muted); font-weight: 500; letter-spacing: 0; text-transform: none; }
    .render { min-height: 96px; display: flex; align-items: center; justify-content: center; overflow: auto; border: 1px solid var(--line); border-radius: 6px; background: #fff; padding: 16px; }
    .render img { max-width: 100%; height: auto; }
    .mjx-render { font-size: 24px; color: #111; }
    .hidden { display: none; }
    @media (max-width: 900px) { .controls, .compare { grid-template-columns: 1fr; } }
  </style>
  <script>
    window.MathJax = {
      tex: {
        inlineMath: [['$', '$'], ['\\\\(', '\\\\)']],
        displayMath: [['$$', '$$'], ['\\\\[', '\\\\]']],
        packages: {'[+]': ['ams', 'color', 'cancel', 'bbox', 'textmacros']}
      },
      loader: {load: ['[tex]/ams', '[tex]/color', '[tex]/cancel', '[tex]/bbox', '[tex]/textmacros']},
      svg: {fontCache: 'global'}
    };
  </script>
  <script defer src="https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-svg.js"></script>
</head>
<body>
  <div class="top">
    <div class="top-inner">
      <h1>mathjax-c 市面常见 LaTeX 数学公式对照</h1>
      <div class="summary">
        <span class="pill">生成时间：${generatedAt}</span>
        <span class="pill">公式：${results.length}</span>
        <span class="pill">测试套件：${results.filter((x) => x.source === 'tools/test.sh').length}</span>
        <span class="pill">追加语料：${results.filter((x) => x.source !== 'tools/test.sh').length}</span>
        <span class="pill">lenient：${lenient}</span>
        <span class="pill">渲染失败：${failures.length}</span>
      </div>
      <div class="controls">
        <input id="q" type="search" placeholder="搜索 name / LaTeX / 分类">
        <select id="section">
          <option value="">全部分类</option>
          ${sections.map((section) => `<option value="${htmlEscape(section)}">${htmlEscape(section)}</option>`).join('\n')}
        </select>
      </div>
    </div>
  </div>
  <main id="cases">
${cards}
  </main>
  <script>
    const q = document.getElementById('q');
    const section = document.getElementById('section');
    const cases = [...document.querySelectorAll('.case')];
    function applyFilter() {
      const needle = q.value.trim().toLowerCase();
      const selected = section.value;
      for (const el of cases) {
        const text = el.textContent.toLowerCase();
        const sectionOk = !selected || el.dataset.section === selected;
        const textOk = !needle || text.includes(needle);
        el.classList.toggle('hidden', !(sectionOk && textOk));
      }
    }
    q.addEventListener('input', applyFilter);
    section.addEventListener('change', applyFilter);
  </script>
</body>
</html>`;

  fs.writeFileSync(path.join(outDir, 'index.html'), html);
}

async function main() {
  if (!fs.existsSync(cli)) throw new Error(`CLI not found: ${cli}`);
  fs.rmSync(outDir, { recursive: true, force: true });
  fs.mkdirSync(assetDir, { recursive: true });
  const cases = buildCases();
  console.log(`Rendering ${cases.length} market/common cases with ${concurrency} workers`);
  const results = await renderAll(cases);
  fs.writeFileSync(path.join(outDir, 'cases.json'), JSON.stringify(results, null, 2));
  writeHtml(results);
  const failed = results.filter((item) => !item.ok);
  console.log(`Done: ${results.length - failed.length}/${results.length} rendered`);
  console.log(`Output: ${path.join(outDir, 'index.html')}`);
  if (failed.length) process.exitCode = 1;
}

main().catch((error) => {
  console.error(error.stack || error.message);
  process.exit(1);
});
