;┌─────────────────────────────────────────────────────────────────────────┐
;│┌───────────   ┌───┐                                                     │
;││              │   │┌──┐                                                 │
;││              │   ││  │┌─┐                                              │
;││              ├───┘│  ││    │                                           │
;││              │    ├──┤│    │    ──                                     │
;││              │    │  ││    │   │                                       │
;││              │    │  ││    └──  ── A :-)                               │
;││ ┌───┐ ┌──┬──┐ ┌───┐ ┌───┐ ┌─── ┌─── ┌───  ─  ┌──┐ ┌──┐                 │
;││ │   │ │  │  │ │   │ │   │ │    │    │        │  │ │  │                 │
;││ │   │ │  │  │ │   │ │   │ │    │    │     │  │  │ │  │                 │
;││ │   │ │     │ ├───┘ ├──┬┘ ├─   └──┐ └──┐  │  │  │ │  │                 │
;││ │   │ │     │ │     │  └│ │       │    │  │  │  │ │  │                 │
;││ │   │ │     │ │     │   │ │       │    │  │  │  │ │  │                 │
;││ └───┘ │     │ │     │   │ └─── ───┘ ───┘  │  └──┘ │  │                 │
;│└────────────────────────────────────────────                            │
;└─────────────────────────────────────────────────────────────────────────┘





		OPTION SCOPED
		.386P
		.model FLAT, C


Set_Raster		MACRO	R,V,B
			push	eax
			push	edx

			mov	dx,3c8h
			xor	al,al
			out	dx,al
			inc	edx

			mov	al,R
			out	dx,al
			mov	al,V
			out	dx,al
			mov	al,B
			out	dx,al

			pop	edx
			pop	eax
			ENDM











;╔═════════════════════════════════════════════════════════════════════════╗
;║ Pour le LZ77                                                            ║
;╚═════════════════════════════════════════════════════════════════════════╝
PUBLIC	C		AddString
PUBLIC	C		DeleteString


Struc_Tree		STRUC
  Parent		dd	0
  Smaller_Child		dd	0
  Larger_Child		dd	0
Struc_Tree		ENDS



INDEX_BIT_COUNT      	=	12
LENGTH_BIT_COUNT     	=	4
WINDOW_SIZE          	=	(1 SHL INDEX_BIT_COUNT)
RAW_LOOK_AHEAD_SIZE  	=	(1 SHL LENGTH_BIT_COUNT)
BREAK_EVEN           	=	( ( 1 + INDEX_BIT_COUNT + LENGTH_BIT_COUNT ) / 9 )
LOOK_AHEAD_SIZE      	=	( RAW_LOOK_AHEAD_SIZE + BREAK_EVEN )
TREE_ROOT            	=	(WINDOW_SIZE) * (Size Struc_Tree)
UNUSED               	=	-1










;╔════════════════════════════════════════════════════════════════════════╗
;║                                                                        ║
;║  Les datas                                                             ║
;║                                                                        ║
;║                                                                        ║
;╚════════════════════════════════════════════════════════════════════════╝
			.DATA

				; *** Pour le LZ77 ***
Extrn	C		tree		:	DWORD
Extrn	C		Current_position:	DWORD
Extrn	C		Match_position	:	DWORD
Extrn	C		window		:	BYTE


Match_Length		dd	0











;╔════════════════════════════════════════════════════════════════════════╗
;║                                                                        ║
;║  Le code                                                               ║
;║                                                                        ║
;║                                                                        ║
;╚════════════════════════════════════════════════════════════════════════╝
			.CODE



;╔═══════════════╤═══════════════════════╦════════════════════════════════╗
;║               │                       ║ Conversion par LCA             ║
;║  Codage LZ77  │                       ║                    le 31-07-95 ║
;╟───────────────┴───────────────────────╨────────────────────────────────╢
;║                                                                        ║
;║ Pour les commentaires, se referer au source LZSS du Data Compression   ║
;║                                                     Book :)            ║
;║                                                                        ║
;╚════════════════════════════════════════════════════════════════════════╝


;╔════════════════════════════════════════════════════════════════════════╗
;║ Recherche si tout ou partie de la chaine en cours se trouve deja dans  ║
;║ le dico, constitue des chaines deja lues.                              ║
;║                                                                        ║
;╚════════════════════════════════════════════════════════════════════════╝
			ALIGN	4
AddString		PROC
			push	ebx
			push	esi
			push	edi
			push	ebp


			mov	ebp,ds:Current_position	; EBP = Current_position
			mov	edi,[ds:tree+TREE_ROOT].Struc_Tree.Larger_Child
			mov	ds:Match_Length,0

			ALIGN	4
@@Main_Loop:
			xor	ecx,ecx			; le compteur: i
			mov	esi,ebp			; ESI:Current_position+i
			mov	ebx,edi			; EBX:Test_Node+i
			xor	edx,edx
@@Same_Char:
			xor	eax,eax

			and	esi,(WINDOW_SIZE-1)	; Le AND pour le modulo
			and	ebx,(WINDOW_SIZE-1)	; dans la fenetre dico

			mov	al,[ds:window+esi]		; EAX=Delta
			mov	dl,[ds:window+ebx]
			inc	esi
			sub	eax,edx
			jne	@@Found

			inc	ecx
			inc	ebx

			cmp	ecx,LOOK_AHEAD_SIZE
			jb	@@Same_Char

@@Found:
			cmp	ecx,ds:Match_Length
			jb	@@Not_Better

			mov	ds:Match_Length,ecx
			cmp	ecx,LOOK_AHEAD_SIZE
			mov	ds:Match_position,edi
			jae	@@Better

			ALIGN	2
@@Not_Better:
			lea	esi,[edi*4]		; ESI:PTR de Test_Node
			test	eax,eax
							; Multiplication par
			lea	esi,[esi+esi*2]		; 12 :) (thanx must fly to Serge!)

			jge	@@PTR_Ok		; Arg!! comment pairer?
			add	esi,(Struc_Tree.Smaller_Child-Struc_Tree.Larger_Child)
@@PTR_Ok:
			lea	esi,[ds:tree+esi].Struc_Tree.Larger_Child

			cmp	dword ptr [esi],UNUSED
			je	@@Add_Leaf

			mov	edi,[esi]
			jmp	@@Main_Loop

							; REPLACE_NODE
			ALIGN	4
@@Better:
			push	Offset @@Exit		; PUSH Imm
			jmp	Replace_Node		; on tombe sur un RET :)

			ALIGN	4
@@Add_Leaf:
			mov	[esi],ebp

			shl	ebp,2
			mov	eax,UNUSED		; Instr. gratuite sur 486 et paire sur P5
			lea	ebp,[ebp+ebp*2]

			mov	[ds:tree+ebp].Struc_Tree.Parent,edi
			mov	[ds:tree+ebp].Struc_Tree.Larger_Child,eax	; et la, on gagne sur le remplissage
			mov	[ds:tree+ebp].Struc_Tree.Smaller_Child,eax	; du prefetch

@@Exit:
			pop	ebp
			pop	edi
			pop	esi
			pop	ebx

			mov	eax,ds:Match_Length

			ret
AddString		ENDP













;╔════════════════════════════════════════════════════════════════════════╗
;║  Remplace un noeud par un nouveau. Utile par exemple, lorsqu'une chaine║
;║ nouvelle est aussi dans le dico, il vaut mieux mettre la nouvelle, car ║
;║ elle sera supprimee apres l'ancienne... mais si c'est clair! :)        ║
;╟────────────────────────────────────────────────────────────────────────╢
;║ Appel : EBP = New_Node (son index)                                     ║
;║         EDI = Old_Node (son index)                                     ║
;║                                                                        ║
;╚════════════════════════════════════════════════════════════════════════╝
			ALIGN   4
Replace_Node		PROC
						; EBP = New_Node : Index
						; EDI = Old_Node : Index
			mov	ebx,ebp
			shl	ebx,2
			lea	ebx,[ebx+ebx*2]	; EBX = New_Node : Deplacement


			mov	edx,edi
			shl	edx,2
			lea	edx,[edx+edx*2]	; EDX = Old_Node : Depl.


			mov	esi,[ds:tree+edx].Struc_Tree.Parent
						; ESI = Parent

			mov	ecx,esi
			shl	ecx,2
			lea	ecx,[ecx+ecx*2]	; ECX = Parent : depl.


			cmp	[ds:tree+ecx].Struc_Tree.Smaller_Child,edi
			jne	@@Not_That_One

			mov	[ds:tree+ecx].Struc_Tree.Smaller_Child,ebp
			jmp	@@Chain_Next
@@Not_That_One:
			mov	[ds:tree+ecx].Struc_Tree.Larger_Child,ebp
@@Chain_Next:

			mov	eax,[ds:tree+edx].Struc_Tree.Parent
			mov	[ds:tree+ebx].Struc_Tree.Parent,eax

			mov	eax,[ds:tree+edx].Struc_Tree.Larger_Child
			mov	[ds:tree+ebx].Struc_Tree.Larger_Child,eax

			mov	eax,[ds:tree+edx].Struc_Tree.Smaller_Child
			mov	[ds:tree+ebx].Struc_Tree.Smaller_Child,eax

			mov	[ds:tree+edx].Struc_Tree.Parent,UNUSED



			cmp	[ds:tree+ebx].Struc_Tree.Smaller_Child,UNUSED
			je	@@Et_Non0
			mov	eax,[ds:tree+ebx].Struc_Tree.Smaller_Child
			shl	eax,2
			lea	eax,[eax+eax*2]
			mov	[ds:tree+eax].Struc_Tree.Parent,ebp
@@Et_Non0:

			cmp	[ds:tree+ebx].Struc_Tree.Larger_Child,UNUSED
			je	@@Exit
			mov	eax,[ds:tree+ebx].Struc_Tree.Larger_Child
			shl	eax,2
			lea	eax,[eax+eax*2]
			mov	[ds:tree+eax].Struc_Tree.Parent,ebp

@@Exit:
			ret
Replace_Node		ENDP








;╔════════════════════════════════════════════════════════════════════════╗
;║  Supprime une chaine dans le dico (pour etre exact, dans l'arbre).     ║
;║ Il faut tout ca, car il faut reordonner l'arbre correctement           ║
;║                                                                        ║
;╟────────────────────────────────────────────────────────────────────────╢
;║ Appel : Passage de parametre par pile: P = Indice du noeud a supprimer ║
;║                                                                        ║
;╚════════════════════════════════════════════════════════════════════════╝
			ALIGN   4
DeleteString		PROC	P:DWORD
			push	edi
			push	esi

			mov	edi,[P]
			mov	esi,[P]
			shl	edi,2
			lea	edi,[edi+edi*2]

			cmp	[ds:tree+edi].Struc_Tree.Parent,UNUSED
			je	@@End

			cmp	[ds:tree+edi].Struc_Tree.Larger_Child,UNUSED
			jne	@@Not_This_One

			mov	eax,[ds:tree+edi].Struc_Tree.Smaller_Child
			push	Offset @@End
			jmp	Contract_Node

			ALIGN	4
@@Not_This_One:
			cmp	[ds:tree+edi].Struc_Tree.Smaller_Child,UNUSED
			jne	@@Not_This_One_Too

			mov	eax,[ds:tree+edi].Struc_Tree.Larger_Child
			push	Offset @@End
			jmp	Contract_Node

			ALIGN	4
@@Not_This_One_Too:
			call	Find_Next_Node

			push	eax
			push	eax
			call	DeleteString
			add	esp,4
			pop	eax

			push	ebx
			push	ebp

			mov	edi,esi
			mov	ebp,eax
			call	Replace_Node

			pop	ebp
			pop	ebx


@@End:
			pop	esi
			pop	edi
			ret
DeleteString		ENDP








;╔════════════════════════════════════════════════════════════════════════╗
;║                                                                        ║
;╟────────────────────────────────────────────────────────────────────────╢
;║ Appel : ESI = Node                                                     ║
;║                                                                        ║
;╚════════════════════════════════════════════════════════════════════════╝
			ALIGN   4
Find_Next_Node		PROC

			lea	ecx,[esi*4]
			lea	ecx,[ecx+ecx*2]

			mov	eax,[ds:tree+ecx].Struc_Tree.Smaller_Child
			ALIGN	4
@@Loop:
			lea	ecx,[eax*4]
			lea	ecx,[ecx+ecx*2]

			cmp	[ds:tree+ecx].Struc_Tree.Larger_Child,UNUSED
			je	@@End

			mov	eax,[ds:tree+ecx].Struc_Tree.Larger_Child
			jmp	@@Loop

@@End:
			ret
Find_Next_Node		ENDP





;╔════════════════════════════════════════════════════════════════════════╗
;║                                                                        ║
;╟────────────────────────────────────────────────────────────────────────╢
;║ Appel : EAX = New Node                                                 ║
;║         ESI = Old Node                                                 ║
;║                                                                        ║
;╚════════════════════════════════════════════════════════════════════════╝
			ALIGN   4
Contract_Node		PROC
			push	ebx

			lea	ebx,[esi*4]
			lea	ebx,[ebx+ebx*2]

			cmp	eax, UNUSED
			je	@@skip1

			lea	ecx,[eax*4]
			lea	ecx,[ecx+ecx*2]

			mov	edx,[ds:tree+ebx].Struc_Tree.Parent
			mov	[ds:tree+ebx].Struc_Tree.Parent,UNUSED
			mov	[ds:tree+ecx].Struc_Tree.Parent,edx
			jmp	@@skip2
@@skip1:
			mov	edx,[ds:tree+ebx].Struc_Tree.Parent
			mov	[ds:tree+ebx].Struc_Tree.Parent,UNUSED
@@skip2:
			shl	edx,2
			lea	edx,[edx+edx*2]


			cmp	[ds:tree+edx].Struc_Tree.Larger_Child,esi
			jne	@@Et_Non
			mov	[ds:tree+edx].Struc_Tree.Larger_Child,eax
			jmp	@@Fin
@@Et_Non:
			mov	[ds:tree+edx].Struc_Tree.Smaller_Child,eax

@@Fin:
			pop	ebx
			ret
Contract_Node		ENDP







;			The
			End

