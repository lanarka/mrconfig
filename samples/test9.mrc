/*
	/*fg*/
	/*
		/
		/*
			NESTED COMMENT
		*/
	
*/

!include "res/colors.mrc"
!use "inc/std.mrc"

[colors]
	white1: 0xffffff
	black1: 0x000000


[desatine] 

	x: -0.01135587
	y: 1.578412019
	z: std.false


[server]
	bind: "192.168.0.1"
	port: 8000

[screen]
	ratio: 0.8
	v: $ABC
	w: "$ABC"

[maps]
	map1: {
		a: 1
		b: 20
		c: 300
		//
		d: {
			e: 1
			f: (
				(
					(
						screen.ratio,1
					)
				)
			)
		}
	}

[ports]
	p1: ((3),2,{
		n:1\
	})

	p2: 0b110011


[server]
	bind1: "192.168.0.1"
	port1: 8000


[cycled]
a: b
b: a


!include "test0.mrc"
!include "test1.mrc"
!include "test2.mrc"
!include "test3.mrc"

// Dup. key: keyQ
//!include "test4.mrc" 

!include "test5.mrc"
!include "test6.mrc"
!include "test7.mrc"
!include "test8.mrc"

!use "inc/math.mrc"

[math]
pi: math.pi
euler: math.euler
maths: (math.pi,math.euler)


!use "res/colors.mrc"

[THEME]
dark: {
	main_window: colors.red
}
light: {
	main_window: colors.blue
}


[ICONS]
ball: (  0x00, 0xFF, \
		 0x00, 0xFF, \
		 0x00, 0xFF  )
logo: (  0x00, 0xFF, \
		 0x00, 0xFF, \
		 0x00, 0xFF  )

x:{a:(1,$ABC\
,3)











}


