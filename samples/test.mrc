
// Texts
!include "res/lang_en.mrc"
!include "res/lang_sk.mrc"

// Icons
!include "res/icons.mrc"

// Themes
!include "res/theme.mrc"

[options]
	foo: {
		bar: 23
	}
	baz: {
		foo: 12
		bar: baz.bar
	}

[baz]
bar: 123
