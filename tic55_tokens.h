#ifndef __USPD_LCD_TOKENS
#define __USPD_LCD_TOKENS


#define token_NULL 0b0000000

/*
A B C D E F G H I J K L M N O P Q R S T U V W X Y Z
*/
#define token_A		0b1111110
#define token_B		0b1111111
#define token_C		0b0011011
#define token_D		0b1111011
#define token_E		0b0011111
#define token_F		0b0011110
#define token_G		0b1011011
#define token_H		0b1101110
#define token_I		0b1100000
#define token_J		0b1100011
#define token_K		0b0101111
#define token_L		0b0001011
#define token_M		0b1110101
#define token_N		0b1111010
#define token_O		0b1111011
#define token_P		0b0111110
#define token_Q		0b1111100
#define token_R		0b0011010
#define token_S		0b1011101
#define token_T		0b1110000
#define token_U		0b1101011
#define token_V		0b0101001
#define token_W		0b0101101
#define token_X		0b0010101
#define token_Y		0b1101101
#define token_Z		0b0110111
/*
a b c d e f g h i j k l m n o p q r s t u v w x y z
*/
#define token_a		0b1110111
#define token_b		0b1001111
#define token_c		0b0000111
#define token_d		0b1100111
#define token_e		0b0111111
#define token_f		token_F
#define token_g		0b1111101
#define token_h		0b1001110
#define token_i		0b0000010
#define token_j		0b1110011
#define token_k		0b0001110
#define token_l		0b0001010
#define token_m		0b0111001
#define token_n		0b1000110
#define token_o		0b1000111
#define token_p		token_P
#define token_q		0b0111101
#define token_r		0b0000110
#define token_s		0b1001100
#define token_t		0b0001111
#define token_u		0b1000011
#define token_v		0b0101100
#define token_w		token_W
#define token_x		token_X
#define token_y		token_Y
#define token_z		token_Z
/*
1 2 3 4 5 6 7 8 9 0
*/
#define token_1		0b1100000
#define token_2		0b0110111
#define token_3		0b1110101
#define token_4		0b1101100
#define token_5		0b1011101
#define token_6		0b1011111
#define token_7		0b1110000
#define token_8		0b1111111
#define token_9		0b1111101
#define token_0		0b1111011

/*
other characters
*/

#define token_zju 0b1001010

#define token_colon	0b0010001
#define token_dash	0b0000100
#define token_space	0b0000000

#define token_gsm_0		token_NULL
#define token_gsm_33	0b0000001
#define token_gsm_66	0b0000101
#define token_gsm_100	0b0010101

#define token_open_bracket	0b0011011
#define token_close_bracket	0b1110001

#endif // __USPD_LCD_TOKENS
