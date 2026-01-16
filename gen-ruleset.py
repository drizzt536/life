"Requires SymPy if you pass anything other than 'B3/S23'"

if __name__ == "__main__":
	from sys import argv
	argv = argv[1:] # ignore the path to this file

	if argv[0].upper() not in {"B3/S23", "B3/S32", "S23/B3", "S32/B3"} \
		and argv[0] not in {"-b", "-s", "-tt1", "-tt2", "-h", "-?", "-help", "--help"}:
		from sympy.logic.boolalg import (
			POSform, SOPform, ANFform, And, Or, Xor, Not, to_nnf,
			BooleanTrue as spTrue, BooleanFalse as spFalse
		)
		from sympy import simplify, symbols, Symbol
		from threading import Thread

		op_class = And | Or | Xor | Not

def safe_simplify(expr, timeout: int = 5):
	"simplify, but cut off if it takes too long"
	result = expr

	def worker() -> None:
		nonlocal result
		result = simplify(expr)

	thread = Thread(target=worker)
	thread.start()
	thread.join(timeout)

	return result

def count_ops(expression) -> int:
	if not isinstance(expression, op_class):
		return 0

	elements = [expression]
	count = 0

	while elements:
		expr = elements.pop()
		count += 1

		for arg in expr.args:
			if isinstance(arg, op_class):
				elements.append(arg)

	return count

def stringify_expr(expr, parent=None) -> str:
	"expr should be op_class. parent should be op_class | None."

	if isinstance(expr, Not):
		return "~" + stringify_expr(expr.args[0])
	elif isinstance(expr, Symbol):
		return str(expr)
	elif isinstance(expr, spTrue | spFalse):
		return str(expr).lower() # c has lowercase `true` and `false`.
	elif isinstance(expr, And | Or | Xor):
		cls = type(expr)
		joiner = {"And": " & ", "Or" : " | ", "Xor": " ^ "}[cls.__name__]
		out = joiner.join(sorted(
			[stringify_expr(arg, parent=cls) for arg in expr.args],
			key=lambda e: str(e).strip("~("),
			reverse=True # reverse order
		))

		if parent is None or parent is Or and cls is And:
			# top level, or something like a & b | c & d
			return out

		# everything else uses parentheses for simplicity
		return f"({out})"
	else:
		raise TypeError(f"unkown class in expression: {type(expr)}")

def tb2expr(ttbits: int) -> str:
	"""
	input is a 16-bit integer encoding the truth table.
	for example, (ttbits >> i) & 1 is the same as truth_table[i]
	"""

	minterms = []
	truth_table = [0]*16 # start with all zeros

	for i in range(16):
		if ttbits & 1:
			minterms.append(i)
			truth_table[i] = 1

		ttbits >>= 1

	# state is called `_s` so it gets placed after n_bitN in reverse ordering.
	vars = symbols("_s n2 n1 n0")

	sop  = safe_simplify(SOPform(vars, minterms))
	pos  = safe_simplify(POSform(vars, minterms))
	anf  = safe_simplify(ANFform(vars, truth_table))
	nnf1 = safe_simplify(to_nnf(sop))
	nnf2 = safe_simplify(to_nnf(pos))

	options = [sop, pos, anf, nnf1, nnf2]

	for i, v in enumerate(options):
		options[i] = v, count_ops(v)

	out = options.pop()

	for opt in options:
		if opt[1] < out[1]:
			out = opt

	return stringify_expr(out[0]).replace("_s", "s")

def str2tb(x: str) -> int:
	A, B = (e for e in x.split("/"))

	# in case they pass S<stuff>/B<stuff> instead of the other way around.

	b_digits = None
	s_digits = None

	if A[0].upper() == 'B':
		b_digits = A[1:]
	elif A[0].upper() == 'S':
		s_digits = A[1:]
	else:
		raise ValueError("unknown character in the first half of the input")

	if B[0].upper() == 'B':
		b_digits = B[1:]
	elif B[0].upper() == 'S':
		s_digits = B[1:]
	else:
		raise ValueError("unknown character in the second half of the input")

	if b_digits is None or s_digits is None:
		raise ValueError("input had two 'B's or two 'S's")

	b = s = 0

	if '0' in b_digits and '8' not in b_digits:
		raise ValueError("0 present in birth cases without an 8")

	if '8' in b_digits and '0' not in b_digits:
		raise ValueError("8 present in birth cases without a 0")

	if '0' in s_digits and '8' not in s_digits:
		raise ValueError("0 present in survive cases without an 8")

	if '8' in s_digits and '0' not in s_digits:
		raise ValueError("8 present in survive cases without a 0")

	for c in b_digits.replace('8', ''):
		b |= 1 << int(c)

	for c in s_digits.replace('8', ''):
		s |= 1 << int(c)

	if b > 0xFF or s > 0xFF:
		raise ValueError("string contained digits higher than 8")

	return s << 8 | b

def str2expr(x: str) -> str:
	return tb2expr(str2tb(x))

if __name__ == "__main__":
	if argv:
		arg0 = argv[0]

	if not argv or arg0 in {"-h", "-?", "-help", "--help"}:
		# birth and survive.
		# no 9s are allowed. 8s should accompany 0s and vice versa
		print(
			"no ruleset or flag provided." \
			"\nexample: `python gen-ruleset.py \"B347/S0238\"`" \
			"\n" \
			"\nuse '-b' before the ruleset to print the B truth table." \
			"\nuse '-s' for the S truth table, and '-tt1' or '-tt2' for both." \
			"\n'-tt1' gives a single truth table, and '-tt2' gives each separately."
		)

		exit(int(not argv))

	if arg0 in {"-b", "-s", "-tt1", "-tt2"}:
		if len(argv) == 1:
			print(f"flag '{arg0}' given without a ruleset")
			exit(1)

		if len(argv) > 2:
			print(f"arguments after the second are ignored.")

		ttbits = str2tb(argv[1])

		b = (ttbits & 1) << 8 | ttbits & 511
		s = ttbits & (1 << 8) | ttbits >> 8

		if arg0 == "-tt1":
			print(f"0b{s << 9 | b:018b}")
		else:
			if arg0 in {"-b", "-tt2"}: print(f"0b{b:09b}")
			if arg0 in {"-s", "-tt2"}: print(f"0b{s:09b}")
	else:
		# print the C condition expression
		if len(argv) > 1:
			print("arguments after the first are ignored.")

		if arg0.upper() in {"B3/S23", "B3/S32", "S23/B3", "S32/B3"}:
			print("~n2 & n1 & (n0 | s)")
		else:
			print(str2expr(arg0))
