; Definition of a sprite

.setcpu "65C02"
.export __tile_data
.segment "RODATA"

__tile_data:
; 256 bytes of fake visible tile for boulder
.byte $66, $66, $66, $66, $66, $66, $66, $66
.byte $66, $66, $66, $66, $66, $66, $66, $66
.byte $66, $66, $66, $66, $66, $66, $66, $66
.byte $66, $66, $66, $66, $66, $66, $66, $66
.byte $66, $66, $66, $66, $66, $66, $66, $66
.byte $66, $66, $66, $66, $66, $66, $66, $66
.byte $66, $66, $66, $66, $66, $66, $66, $66
.byte $66, $66, $66, $66, $66, $66, $66, $66
.byte $66, $66, $66, $66, $66, $66, $66, $66
.byte $66, $66, $66, $66, $66, $66, $66, $66
.byte $66, $66, $66, $66, $66, $66, $66, $66
.byte $66, $66, $66, $66, $66, $66, $66, $66
.byte $66, $66, $66, $66, $66, $66, $66, $66
.byte $66, $66, $66, $66, $66, $66, $66, $66
.byte $66, $66, $66, $66, $66, $66, $66, $66
.byte $66, $66, $66, $66, $66, $66, $66, $66

