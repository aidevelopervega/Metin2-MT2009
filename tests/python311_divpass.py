"""MT2009 - Python 2 klasik '/' semantigini Python 3'te AYNEN koruyan statik donusum.

Python 2:  int / int  -> taban bolme (floor) ;  herhangi biri float ise gercek bolme.
Python 3:  '/' her zaman gercek bolme  -> UI koordinatlari float olur, C++ 'PyLong_AsLong'
           float'u reddeder (-1 + bekleyen istisna), liste indeksi TypeError verir.

Her '/' noktasi icin operand tipleri statik olarak cikarilir (I=int, F=float, U=bilinmiyor):
  I / I  -> '//'            (kanitli int: Python 2'de taban bolmeydi)
  F evidan (herhangi bir taraf) -> '/' degismez (Python 2'de gercek bolmeydi)
  digerleri -> _py2div(a, b) (dosya-yerel yardimci; runtime'da tam Python 2 semantigi)
Yardimci YALNIZCA tipi kanitlanamayan noktalarda kullanilir; her nokta rapora yazilir.
Girdi metni LF satir sonlu olmalidir.
"""
import ast
import re

HELPER = (
    "def _py2div(a, b):\n"
    "\t# Python 2 klasik '/' (int/int -> taban bolme, aksi halde gercek bolme); yalniz tipi kanitlanamayan noktalar.\n"
    "\tif isinstance(a, int) and isinstance(b, int):\n"
    "\t\treturn a // b\n"
    "\treturn a / b\n"
    "\n"
)

INT_NAMES = {"SCREEN_WIDTH", "SCREEN_HEIGHT"}  # ui.PythonScriptLoader.Clear(): wndMgr.GetScreenWidth()/Height() -> int
INT_METHODS = {"GetWidth", "GetHeight", "GetScreenWidth", "GetScreenHeight", "GetItemCount", "GetViewItemCount", "GetLineCount"}
FLOAT_MATH = {"sqrt", "sin", "cos", "tan", "atan", "atan2", "asin", "acos", "pow", "exp", "log", "log10", "floor", "ceil", "fabs", "radians", "degrees", "hypot", "fmod"}
I, F, U, BOT = "I", "F", "U", "B"


def join(a, b):
    if a == BOT:
        return b
    if b == BOT:
        return a
    return a if a == b else U


class Scope:
    def __init__(self, parent=None, is_class=False):
        self.parent = parent
        self.types = {}
        self.unknown = set()
        self.is_class = is_class

    def lookup(self, name):
        s = self
        while s is not None:
            if not s.is_class or s is self:
                if name in s.unknown:
                    return U
                if name in s.types:
                    return s.types[name]
            s = s.parent
        return I if name in INT_NAMES else U


def _targets(t, out):
    if isinstance(t, ast.Name):
        out.append(t)
    elif isinstance(t, (ast.Tuple, ast.List)):
        for e in t.elts:
            _targets(e, out)
    elif isinstance(t, ast.Starred):
        _targets(t.value, out)


class Typer:
    def __init__(self, tree):
        self.tree = tree
        self.scope_of = {}
        self.global_names = set()
        for n in ast.walk(tree):
            if isinstance(n, ast.Global):
                self.global_names.update(n.names)
        self._build(tree, Scope())

    def _own_nodes(self, node):
        """node govdesindeki, ic ice fonksiyon/sinif/lambda'ya girmeden dugumler."""
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            stack = list(node.body)
        elif isinstance(node, ast.Lambda):
            stack = [node.body]
        else:
            stack = list(ast.iter_child_nodes(node))
        while stack:
            n = stack.pop()
            yield n
            if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef, ast.Lambda)):
                continue
            stack.extend(ast.iter_child_nodes(n))

    def _build(self, node, scope):
        self.scope_of[node] = scope
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.Lambda)):
            a = node.args
            for arg in a.posonlyargs + a.args + a.kwonlyargs + ([a.vararg] if a.vararg else []) + ([a.kwarg] if a.kwarg else []):
                scope.unknown.add(arg.arg)
        assigns = []
        nested = []
        for n in self._own_nodes(node):
            if isinstance(n, ast.Assign):
                for t in n.targets:
                    names = []
                    _targets(t, names)
                    for nm in names:
                        if isinstance(t, ast.Name):
                            assigns.append((nm.id, n.value, None))
                        else:
                            scope.unknown.add(nm.id)
            elif isinstance(n, ast.AugAssign) and isinstance(n.target, ast.Name):
                assigns.append((n.target.id, n.value, n.op))
            elif isinstance(n, ast.AnnAssign) and isinstance(n.target, ast.Name) and n.value is not None:
                assigns.append((n.target.id, n.value, None))
            elif isinstance(n, (ast.For, ast.AsyncFor)):
                names = []
                _targets(n.target, names)
                it = n.iter
                is_range = (isinstance(it, ast.Call) and isinstance(it.func, ast.Name) and it.func.id == "range"
                            and isinstance(n.target, ast.Name))
                for nm in names:
                    if is_range:
                        assigns.append((nm.id, "RANGE", None))
                    else:
                        scope.unknown.add(nm.id)
            elif isinstance(n, (ast.With, ast.AsyncWith)):
                for it in n.items:
                    if it.optional_vars is not None:
                        names = []
                        _targets(it.optional_vars, names)
                        scope.unknown.update(x.id for x in names)
            elif isinstance(n, ast.ExceptHandler) and n.name:
                scope.unknown.add(n.name)
            elif isinstance(n, (ast.Import, ast.ImportFrom)):
                for al in n.names:
                    scope.unknown.add((al.asname or al.name).split(".")[0])
            elif isinstance(n, (ast.Global, ast.Nonlocal)):
                scope.unknown.update(n.names)
            elif isinstance(n, ast.NamedExpr):
                scope.unknown.add(n.target.id)
            if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)):
                scope.unknown.add(n.name)
                nested.append(n)
            elif isinstance(n, ast.ClassDef):
                scope.unknown.add(n.name)
                nested.append(n)
            elif isinstance(n, ast.Lambda):
                nested.append(n)
        if isinstance(node, ast.Module):
            scope.unknown.update(self.global_names)
        for nm, _, _ in assigns:
            scope.types.setdefault(nm, BOT)
        for _ in range(6):
            changed = False
            for nm, val, op in assigns:
                if nm in scope.unknown:
                    continue
                if isinstance(val, str):
                    t = I
                elif op is not None:
                    t = self._binop_type(op, scope.types.get(nm, BOT), self.ty(val, scope))
                else:
                    t = self.ty(val, scope)
                new = join(scope.types[nm], t)
                if new != scope.types[nm]:
                    scope.types[nm] = new
                    changed = True
            if not changed:
                break
        for nm in list(scope.types):
            if scope.types[nm] == BOT:
                scope.types[nm] = U
        for n in nested:
            self._build(n, Scope(scope, is_class=isinstance(n, ast.ClassDef)))

    @staticmethod
    def _binop_type(op, a, b):
        if a == BOT or b == BOT:
            return BOT
        if isinstance(op, ast.Div):
            return F if (a == F or b == F) else (I if (a == I and b == I) else U)
        if isinstance(op, (ast.Add, ast.Sub, ast.Mult, ast.Mod, ast.FloorDiv)):
            if a == F or b == F:
                return F
            return I if (a == I and b == I) else U
        if isinstance(op, ast.Pow):
            return F if (a == F or b == F) else U
        if isinstance(op, (ast.LShift, ast.RShift, ast.BitAnd, ast.BitOr, ast.BitXor)):
            return I if (a == I and b == I) else U
        return U

    def ty(self, n, scope):
        if isinstance(n, ast.Constant):
            v = n.value
            if isinstance(v, (bool, int)):
                return I
            if isinstance(v, float):
                return F
            return U
        if isinstance(n, ast.Name):
            return scope.lookup(n.id)
        if isinstance(n, ast.UnaryOp):
            return self.ty(n.operand, scope) if isinstance(n.op, (ast.UAdd, ast.USub)) else U
        if isinstance(n, ast.BinOp):
            return self._binop_type(n.op, self.ty(n.left, scope), self.ty(n.right, scope))
        if isinstance(n, ast.IfExp):
            return join(self.ty(n.body, scope), self.ty(n.orelse, scope))
        if isinstance(n, ast.Attribute):
            if isinstance(n.value, ast.Name) and n.value.id == "math" and n.attr in ("pi", "e", "tau", "inf"):
                return F
            return U
        if isinstance(n, ast.Call):
            f = n.func
            if isinstance(f, ast.Name):
                if f.id == "float":
                    return F
                if f.id in ("int", "len", "ord"):
                    return I
                if f.id == "round" and len(n.args) == 1 and not n.keywords:
                    return F  # Python 2'de round() float dondurur
                if f.id == "abs" and len(n.args) == 1:
                    return self.ty(n.args[0], scope)
                if f.id in ("max", "min") and n.args and not n.keywords:
                    t = BOT
                    for a in n.args:
                        t = join(t, self.ty(a, scope))
                    return t if t != BOT else U
            if isinstance(f, ast.Attribute):
                if isinstance(f.value, ast.Name) and f.value.id == "math" and f.attr in FLOAT_MATH:
                    return F  # Python 2'de math.floor/ceil de float dondurur
                if f.attr in INT_METHODS:
                    return I
            return U
        return U


def _line_starts(b):
    starts = [0]
    for m in re.finditer(b"\n", b):
        starts.append(m.end())
    return starts


def rewrite_division(text, name):
    """-> (yeni_metin, stats)"""
    stats = {"floor": 0, "true": 0, "helper": 0, "helper_sites": [], "manual": []}
    tree = ast.parse(text)
    typer = Typer(tree)
    b = text.encode("utf-8")
    ls = _line_starts(b)

    def off(line, col):
        return ls[line - 1] + col

    def line_text(lineno):
        end = ls[lineno] - 1 if lineno < len(ls) else len(b)
        return b[ls[lineno - 1]:end].decode("utf-8").strip()[:140]

    edits = []

    def visit(node, scope):
        sc = typer.scope_of.get(node, scope)
        for child in ast.iter_child_nodes(node):
            visit(child, sc)
        if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Div):
            ta, tb = typer.ty(node.left, sc), typer.ty(node.right, sc)
            lend = off(node.left.end_lineno, node.left.end_col_offset)
            rstart = off(node.right.lineno, node.right.col_offset)
            between = b[lend:rstart]
            pos = between.find(b"/")
            if pos < 0 or b"#" in between or between.count(b"/") != 1:
                stats["manual"].append((node.lineno, "operator konumu belirsiz"))
                return
            op = lend + pos
            s, e = off(node.lineno, node.col_offset), off(node.end_lineno, node.end_col_offset)
            if ta == F or tb == F:
                stats["true"] += 1
            elif ta == I and tb == I:
                edits.append((op, op + 1, "floor", None))
                stats["floor"] += 1
            else:
                edits.append((s, e, "helper", op))
                stats["helper"] += 1
                stats["helper_sites"].append((node.lineno, line_text(node.lineno)))
        elif isinstance(node, ast.AugAssign) and isinstance(node.op, ast.Div) and isinstance(node.target, ast.Name):
            ta = sc.lookup(node.target.id)
            tb = typer.ty(node.value, sc)
            tend = off(node.target.end_lineno, node.target.end_col_offset)
            vstart = off(node.value.lineno, node.value.col_offset)
            between = b[tend:vstart]
            pos = between.find(b"/=")
            if pos < 0 or b"#" in between:
                stats["manual"].append((node.lineno, "aug-op konumu belirsiz"))
                return
            op = tend + pos
            s, e = off(node.lineno, node.col_offset), off(node.end_lineno, node.end_col_offset)
            if ta == F or tb == F:
                stats["true"] += 1
            elif ta == I and tb == I:
                edits.append((op, op + 2, "floor_aug", None))
                stats["floor"] += 1
            else:
                edits.append((s, e, "helper_aug", op))
                stats["helper"] += 1
                stats["helper_sites"].append((node.lineno, "(aug) " + line_text(node.lineno)))

    visit(tree, None)

    def render(s, e):
        inside = sorted((x for x in edits if x[0] >= s and x[1] <= e), key=lambda x: (x[0], -x[1]))
        out, pos, last_end = [], s, s
        for ed in inside:
            if ed[0] < last_end:
                continue
            out.append(b[pos:ed[0]])
            st, en, kind, op = ed
            if kind == "floor":
                out.append(b"//")
            elif kind == "floor_aug":
                out.append(b"//=")
            elif kind == "helper":
                out.append(b"_py2div(" + render(st, op).strip() + b", " + render(op + 1, en).strip() + b")")
            elif kind == "helper_aug":
                tgt = b[st:op].strip()
                out.append(tgt + b" = _py2div(" + tgt + b", " + render(op + 2, en).strip() + b")")
            pos = last_end = en
        out.append(b[pos:e])
        return b"".join(out)

    new = render(0, len(b)).decode("utf-8")
    if stats["helper"]:
        new = HELPER + new
    return new, stats
