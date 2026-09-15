[foo]
init: 0

[bar]
root: {
	baz: 2
	arr: (1, 2, 3, 4)
	title: "Hello"
	nested: {
		Array: (10, 11, 12, 13, 14, 15, (16,16))
		fox: "Fox"
		nested: {
			f1: bar.root.nested.fox
			f2: bar.root.nested.Array.6.0
		}
	}
}
