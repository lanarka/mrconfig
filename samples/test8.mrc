/*
/*
//
/*/
*/



/* 

Multi
	Line



	 Comment 
*/

!use "inc/std.mrc"
!use "res/colors.mrc"

[sekcia1] // Line comment

key1 /*JM*/: 55
keyX: std.true
keyY: std.false
keyZ: std.null
key2: 55.1
key3: (1,1,\
	5  )
key4: "Hello"

key5: (1.1, \
	0b00000001,\
   0x03 ,"e")

key6: "xxx" \
	  "xxE"

[new]
color: colors.red

[textsa]
escape: "Hello \"World\""

/*
	zzzzz
	?????
*/


[Maps]
mymap: {a: 123
        b: -456
        c: "Hello World"
        d: 1.17
        e: -123.5

        f: 0xff
        g: (10, 20 , 30 ,40)
        h: sekcia1.key5.1

        i: (1, 0b1111 , 3, 0xff)
        j: {a: -12.34}
        k: -1
        l: {a:1}
}



[new2]
color: colors.red

[texts]
longText: "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum." \

more_texts: "Lorem " \
	"ipsum " "dolor " \
	"sit " "amet"\
	"."

escape: ("Hello \"World\"", Maps.mymap)

special_chars: ("♡", "⚠️", {a:1   
	b :(-2,(special_chars.2,1)) \
	c: 2})
/**/

ggg:/*stress*/ (0/*stress*/,escape/*stress*/.0/**/,\
/**/							)
/*stress*/

/*stress*//*stress*//*stress*//*stress*//*stress*/
[wow]
uuu: (1/*stress*/,2/*stress*/,3/*stress*/,/*stress*//*stress*/(1,2))/*stress*/
/*stress*/uu:/*stress*/ (wow.uuu.3.0,/*stress*/    1,colors.red)/*stress*/
[k]/*stress*/
/*stress*/a: \
(\
1\
\
,\
(\
/*stress*/1\
/*stress*/\
/*stress*/,\
\
\
(                                                               1/*2*/)\
)\
)




[data]
my_Bytes: (	0x00,0x00,0x00,\
	0x00,0x00,0x00 )

