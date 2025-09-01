/*
		.386p
		.model	SMALL, C

		.data

		public	C	SinTabF
		public	C	CosTabF

ALIGN	4

SinTabF	label	DWORD

; Sinus Table: 5120 32bits float values
dd	000000000h, 03ac90fd5h, 03b490fc6h, 03b96cbc1h, 03bc90f88h, 03bfb5330h
dd	03c16cb58h, 03c2fed02h, 03c490e90h, 03c622fffh, 03c7b514bh, 03c8a3938h
dd	03c96c9b6h, 03ca35a1ch, 03cafea69h, 03cbc7a9bh, 03cc90ab0h, 03cd59aa6h
dd	03ce22a7ah, 03ceeba2ch, 03cfb49bah, 03d03ec90h, 03d0a342fh, 03d107bb8h
dd	03d16c32ch, 03d1d0a88h, 03d2351cbh, 03d2998f6h, 03d2fe007h, 03d3626fch
dd	03d3c6dd5h, 03d42b491h, 03d48fb30h, 03d4f41afh, 03d55880eh, 03d5bce4ch
dd	03d621469h, 03d685a62h, 03d6ea038h, 03d74e5e9h, 03d7b2b74h, 03d80b86ch
dd	03d83db0ah, 03d86fd94h, 03d8a200ah, 03d8d426ah, 03d9064b4h, 03d9386e7h
dd	03d96a905h, 03d99cb0ah, 03d9cecf9h, 03da00ecfh, 03da3308ch, 03da65230h
dd	03da973bah, 03dac952bh, 03dafb680h, 03db2d7bbh, 03db5f8dah, 03db919ddh
dd	03dbc3ac3h, 03dbf5b8dh, 03dc27c39h, 03dc59cc6h, 03dc8bd36h, 03dcbdd86h
dd	03dcefdb7h, 03dd21dc8h, 03dd53db9h, 03dd85d89h, 03ddb7d37h, 03dde9cc4h
dd	03de1bc2eh, 03de4db76h, 03de7fa9ah, 03deb199ah, 03dee3876h, 03df1572eh
dd	03df475c0h, 03df7942ch, 03dfab273h, 03dfdd092h, 03e007745h, 03e02062eh
dd	03e039502h, 03e0523c2h, 03e06b26eh, 03e084105h, 03e09cf86h, 03e0b5df3h
dd	03e0cec4ah, 03e0e7a8bh, 03e1008b7h, 03e1196cch, 03e1324cah, 03e14b2b2h
dd	03e164083h, 03e17ce3dh, 03e195be0h, 03e1ae96bh, 03e1c76deh, 03e1e0438h
dd	03e1f917bh, 03e211ea5h, 03e22abb6h, 03e2438adh, 03e25c58ch, 03e275251h
dd	03e28defch, 03e2a6b8dh, 03e2bf804h, 03e2d8461h, 03e2f10a2h, 03e309cc9h
dd	03e3228d4h, 03e33b4c4h, 03e354098h, 03e36cc50h, 03e3857ech, 03e39e36ch
dd	03e3b6ecfh, 03e3cfa15h, 03e3e853eh, 03e401049h, 03e419b37h, 03e432607h
dd	03e44b0b9h, 03e463b4dh, 03e47c5c2h, 03e495018h, 03e4ada4fh, 03e4c6467h
dd	03e4dee60h, 03e4f7838h, 03e5101f1h, 03e528b89h, 03e541501h, 03e559e58h
dd	03e57278fh, 03e58b0a4h, 03e5a3997h, 03e5bc26ah, 03e5d4b1ah, 03e5ed3a8h
dd	03e605c13h, 03e61e45ch, 03e636c83h, 03e64f486h, 03e667c66h, 03e680422h
dd	03e698bbah, 03e6b132fh, 03e6c9a7fh, 03e6e21abh, 03e6fa8b2h, 03e712f94h
dd	03e72b651h, 03e743ce8h, 03e75c35ah, 03e7749a6h, 03e78cfcch, 03e7a55cbh
dd	03e7bdba4h, 03e7d6156h, 03e7ee6e1h, 03e803622h, 03e80f8c0h, 03e81bb4ah
dd	03e827dc0h, 03e834022h, 03e840270h, 03e84c4aah, 03e8586ceh, 03e8648dfh
dd	03e870adah, 03e87ccc1h, 03e888e93h, 03e895050h, 03e8a11f7h, 03e8ad38ah
dd	03e8b9507h, 03e8c566eh, 03e8d17c0h, 03e8dd8fch, 03e8e9a22h, 03e8f5b32h
dd	03e901c2ch, 03e90dd10h, 03e919dddh, 03e925e94h, 03e931f35h, 03e93dfbfh
dd	03e94a031h, 03e95608dh, 03e9620d2h, 03e96e100h, 03e97a117h, 03e986116h
dd	03e9920feh, 03e99e0ceh, 03e9aa086h, 03e9b6027h, 03e9c1fafh, 03e9cdf20h
dd	03e9d9e78h, 03e9e5db8h, 03e9f1cdfh, 03e9fdbeeh, 03ea09ae5h, 03ea159c2h
dd	03ea21887h, 03ea2d733h, 03ea395c5h, 03ea4543fh, 03ea5129fh, 03ea5d0e5h
dd	03ea68f12h, 03ea74d25h, 03ea80b1fh, 03ea8c8feh, 03ea986c4h, 03eaa446fh
dd	03eab0201h, 03eabbf77h, 03eac7cd4h, 03ead3a15h, 03eadf73ch, 03eaeb449h
dd	03eaf713ah, 03eb02e10h, 03eb0eacbh, 03eb1a76bh, 03eb263efh, 03eb32058h
dd	03eb3dca5h, 03eb498d6h, 03eb554ech, 03eb610e6h, 03eb6ccc3h, 03eb78884h
dd	03eb8442ah, 03eb8ffb2h, 03eb9bb1eh, 03eba766eh, 03ebb31a0h, 03ebbecb6h
dd	03ebca7afh, 03ebd628bh, 03ebe1d4ah, 03ebed7ebh, 03ebf926fh, 03ec04cd5h
dd	03ec1071eh, 03ec1c148h, 03ec27b55h, 03ec33544h, 03ec3ef15h, 03ec4a8c8h
dd	03ec5625ch, 03ec61bd2h, 03ec6d529h, 03ec78e62h, 03ec8477ch, 03ec90077h
dd	03ec9b953h, 03eca7210h, 03ecb2aaeh, 03ecbe32ch, 03ecc9b8bh, 03ecd53cah
dd	03ece0beah, 03ecec3eah, 03ecf7bcah, 03ed0338ah, 03ed0eb2ah, 03ed1a2aah
dd	03ed25a09h, 03ed31148h, 03ed3c867h, 03ed47f64h, 03ed53641h, 03ed5ecfdh
dd	03ed6a399h, 03ed75a13h, 03ed8106bh, 03ed8c6a3h, 03ed97cb9h, 03eda32adh
dd	03edae880h, 03edb9e31h, 03edc53c1h, 03edd092eh, 03eddbe79h, 03ede73a2h
dd	03edf28a9h, 03edfdd8dh, 03ee0924fh, 03ee146eeh, 03ee1fb6ah, 03ee2afc4h
dd	03ee363fah, 03ee4180eh, 03ee4cbfeh, 03ee57fcbh, 03ee63375h, 03ee6e6fbh
dd	03ee79a5dh, 03ee84d9ch, 03ee900b7h, 03ee9b3aeh, 03eea6681h, 03eeb1930h
dd	03eebcbbbh, 03eec7e21h, 03eed3063h, 03eede280h, 03eee9479h, 03eef464ch
dd	03eeff7fbh, 03ef0a985h, 03ef15aeah, 03ef20c29h, 03ef2bd43h, 03ef36e38h
dd	03ef41f07h, 03ef4cfb1h, 03ef58035h, 03ef63093h, 03ef6e0cbh, 03ef790dch
dd	03ef840c8h, 03ef8f08eh, 03ef9a02dh, 03efa4fa5h, 03efafef7h, 03efbae22h
dd	03efc5d27h, 03efd0c04h, 03efdbabbh, 03efe694ah, 03eff17b2h, 03effc5f3h
dd	03f003a06h, 03f0090ffh, 03f00e7e4h, 03f013eb5h, 03f019573h, 03f01ec1ch
dd	03f0242b1h, 03f029932h, 03f02ef9fh, 03f0345f8h, 03f039c3dh, 03f03f26dh
dd	03f044889h, 03f049e91h, 03f04f484h, 03f054a62h, 03f05a02ch, 03f05f5e2h
dd	03f064b82h, 03f06a10eh, 03f06f686h, 03f074be8h, 03f07a136h, 03f07f66fh
dd	03f084b92h, 03f08a0a1h, 03f08f59bh, 03f094a7fh, 03f099f4eh, 03f09f409h
dd	03f0a48adh, 03f0a9d3dh, 03f0af1b7h, 03f0b461ch, 03f0b9a6bh, 03f0beea5h
dd	03f0c42c9h, 03f0c96d7h, 03f0cead0h, 03f0d3eb3h, 03f0d9281h, 03f0de638h
dd	03f0e39dah, 03f0e8d65h, 03f0ee0dbh, 03f0f343bh, 03f0f8784h, 03f0fdab8h
dd	03f102dd5h, 03f1080dch, 03f10d3cdh, 03f1126a7h, 03f11796bh, 03f11cc19h
dd	03f121eb0h, 03f127130h, 03f12c39ah, 03f1315eeh, 03f13682ah, 03f13ba50h
dd	03f140c5fh, 03f145e58h, 03f14b039h, 03f150204h, 03f1553b7h, 03f15a554h
dd	03f15f6d9h, 03f164847h, 03f16999fh, 03f16eadeh, 03f173c07h, 03f178d18h
dd	03f17de12h, 03f182ef5h, 03f187fc0h, 03f18d073h, 03f19210fh, 03f197194h
dd	03f19c200h, 03f1a1255h, 03f1a6293h, 03f1ab2b8h, 03f1b02c6h, 03f1b52bbh
dd	03f1ba299h, 03f1bf25fh, 03f1c420ch, 03f1c91a2h, 03f1ce11fh, 03f1d3084h
dd	03f1d7fd1h, 03f1dcf06h, 03f1e1e22h, 03f1e6d26h, 03f1ebc12h, 03f1f0ae5h
dd	03f1f599fh, 03f1fa841h, 03f1ff6cbh, 03f20453bh, 03f209393h, 03f20e1d2h
dd	03f212ff9h, 03f217e06h, 03f21cbfbh, 03f2219d7h, 03f226799h, 03f22b543h
dd	03f2302d3h, 03f23504bh, 03f239da9h, 03f23eaeeh, 03f24381ah, 03f24852ch
dd	03f24d225h, 03f251f04h, 03f256bcbh, 03f25b877h, 03f26050ah, 03f265184h
dd	03f269de3h, 03f26ea2ah, 03f273656h, 03f278268h, 03f27ce61h, 03f281a40h
dd	03f286605h, 03f28b1b0h, 03f28fd41h, 03f2948b8h, 03f299415h, 03f29df57h
dd	03f2a2a80h, 03f2a758eh, 03f2ac082h, 03f2b0b5bh, 03f2b561bh, 03f2ba0bfh
dd	03f2beb4ah, 03f2c35b9h, 03f2c800fh, 03f2cca49h, 03f2d1469h, 03f2d5e6fh
dd	03f2da859h, 03f2df229h, 03f2e3bdeh, 03f2e8578h, 03f2ecef7h, 03f2f185bh
dd	03f2f61a5h, 03f2faad3h, 03f2ff3e6h, 03f303cdeh, 03f3085bbh, 03f30ce7ch
dd	03f311722h, 03f315fadh, 03f31a81dh, 03f31f071h, 03f3238aah, 03f3280c7h
dd	03f32c8c9h, 03f3310afh, 03f33587ah, 03f33a029h, 03f33e7bch, 03f342f34h
dd	03f34768fh, 03f34bdcfh, 03f3504f3h, 03f354bfbh, 03f3592e7h, 03f35d9b8h
dd	03f36206ch, 03f366704h, 03f36ad7fh, 03f36f3dfh, 03f373a23h, 03f37804ah
dd	03f37c655h, 03f380c43h, 03f385216h, 03f3897cbh, 03f38dd65h, 03f3922e1h
dd	03f396842h, 03f39ad85h, 03f39f2ach, 03f3a37b7h, 03f3a7ca4h, 03f3ac175h
dd	03f3b0629h, 03f3b4ac1h, 03f3b8f3bh, 03f3bd398h, 03f3c17d9h, 03f3c5bfch
dd	03f3ca003h, 03f3ce3ech, 03f3d27b8h, 03f3d6b67h, 03f3daef9h, 03f3df26eh
dd	03f3e35c5h, 03f3e78ffh, 03f3ebc1bh, 03f3eff1bh, 03f3f41fch, 03f3f84c0h
dd	03f3fc767h, 03f4009f0h, 03f404c5ch, 03f408ea9h, 03f40d0dah, 03f4112ech
dd	03f4154e1h, 03f4196b7h, 03f41d870h, 03f421a0bh, 03f425b89h, 03f429ce8h
dd	03f42de29h, 03f431f4ch, 03f436051h, 03f43a138h, 03f43e200h, 03f4422abh
dd	03f446337h, 03f44a3a5h, 03f44e3f5h, 03f452426h, 03f456439h, 03f45a42dh
dd	03f45e403h, 03f4623bbh, 03f466354h, 03f46a2ceh, 03f46e22ah, 03f472167h
dd	03f476085h, 03f479f84h, 03f47de65h, 03f481d27h, 03f485bcah, 03f489a4eh
dd	03f48d8b3h, 03f4916fah, 03f495521h, 03f499329h, 03f49d112h, 03f4a0edch
dd	03f4a4c87h, 03f4a8a13h, 03f4ac77fh, 03f4b04cch, 03f4b41fah, 03f4b7f09h
dd	03f4bbbf8h, 03f4bf8c7h, 03f4c3578h, 03f4c7208h, 03f4cae79h, 03f4ceacbh
dd	03f4d26fdh, 03f4d6310h, 03f4d9f02h, 03f4ddad5h, 03f4e1689h, 03f4e521ch
dd	03f4e8d90h, 03f4ec8e4h, 03f4f0417h, 03f4f3f2bh, 03f4f7a1fh, 03f4fb4f4h
dd	03f4fefa8h, 03f502a3bh, 03f5064afh, 03f509f03h, 03f50d937h, 03f51134ah
dd	03f514d3dh, 03f518710h, 03f51c0c2h, 03f51fa54h, 03f5233c6h, 03f526d18h
dd	03f52a649h, 03f52df59h, 03f531849h, 03f535118h, 03f5389c7h, 03f53c255h
dd	03f53fac3h, 03f54330fh, 03f546b3bh, 03f54a347h, 03f54db31h, 03f5512fbh
dd	03f554aa4h, 03f55822ch, 03f55b993h, 03f55f0d9h, 03f5627feh, 03f565f02h
dd	03f5695e5h, 03f56cca7h, 03f570348h, 03f5739c7h, 03f577026h, 03f57a663h
dd	03f57dc7fh, 03f581279h, 03f584853h, 03f587e0bh, 03f58b3a1h, 03f58e916h
dd	03f591e6ah, 03f59539ch, 03f5988adh, 03f59bd9ch, 03f59f26ah, 03f5a2716h
dd	03f5a5ba0h, 03f5a9009h, 03f5ac450h, 03f5af875h, 03f5b2c79h, 03f5b605ah
dd	03f5b941ah, 03f5bc7b8h, 03f5bfb34h, 03f5c2e8eh, 03f5c61c7h, 03f5c94ddh
dd	03f5cc7d1h, 03f5cfaa3h, 03f5d2d53h, 03f5d5fe1h, 03f5d924dh, 03f5dc497h
dd	03f5df6beh, 03f5e28c3h, 03f5e5aa6h, 03f5e8c67h, 03f5ebe05h, 03f5eef81h
dd	03f5f20dbh, 03f5f5212h, 03f5f8327h, 03f5fb419h, 03f5fe4e9h, 03f601596h
dd	03f604621h, 03f607689h, 03f60a6cfh, 03f60d6f2h, 03f6106f2h, 03f6136d0h
dd	03f61668ah, 03f619622h, 03f61c598h, 03f61f4eah, 03f62241ah, 03f625326h
dd	03f628210h, 03f62b0d7h, 03f62df7bh, 03f630dfch, 03f633c5ah, 03f636a95h
dd	03f6398ach, 03f63c6a1h, 03f63f473h, 03f642221h, 03f644fach, 03f647d14h
dd	03f64aa59h, 03f64d77bh, 03f650479h, 03f653154h, 03f655e0bh, 03f658aa0h
dd	03f65b710h, 03f65e35eh, 03f660f88h, 03f663b8eh, 03f666771h, 03f669330h
dd	03f66becch, 03f66ea45h, 03f671599h, 03f6740cah, 03f676bd8h, 03f6796c1h
dd	03f67c187h, 03f67ec29h, 03f6816a8h, 03f684103h, 03f686b39h, 03f68954ch
dd	03f68bf3ch, 03f68e907h, 03f6912aeh, 03f693c32h, 03f696591h, 03f698ecch
dd	03f69b7e4h, 03f69e0d7h, 03f6a09a7h, 03f6a3252h, 03f6a5ad9h, 03f6a833ch
dd	03f6aab7bh, 03f6ad395h, 03f6afb8ch, 03f6b235eh, 03f6b4b0ch, 03f6b7295h
dd	03f6b99fbh, 03f6bc13bh, 03f6be858h, 03f6c0f50h, 03f6c3624h, 03f6c5cd4h
dd	03f6c835eh, 03f6ca9c5h, 03f6cd007h, 03f6cf624h, 03f6d1c1dh, 03f6d41f2h
dd	03f6d67a1h, 03f6d8d2dh, 03f6db293h, 03f6dd7d5h, 03f6dfcf2h, 03f6e21ebh
dd	03f6e46beh, 03f6e6b6dh, 03f6e8ff8h, 03f6eb45dh, 03f6ed89eh, 03f6efcbah
dd	03f6f20b0h, 03f6f4483h, 03f6f6830h, 03f6f8bb8h, 03f6faf1bh, 03f6fd25ah
dd	03f6ff573h, 03f701867h, 03f703b37h, 03f705de1h, 03f708066h, 03f70a2c6h
dd	03f70c501h, 03f70e717h, 03f710908h, 03f712ad4h, 03f714c7ah, 03f716dfbh
dd	03f718f57h, 03f71b08eh, 03f71d19fh, 03f71f28ch, 03f721352h, 03f7233f4h
dd	03f725470h, 03f7274c7h, 03f7294f8h, 03f72b504h, 03f72d4ebh, 03f72f4ach
dd	03f731447h, 03f7333beh, 03f73530eh, 03f737239h, 03f73913fh, 03f73b01fh
dd	03f73ced9h, 03f73ed6eh, 03f740bddh, 03f742a27h, 03f74484bh, 03f746649h
dd	03f748422h, 03f74a1d5h, 03f74bf62h, 03f74dcc9h, 03f74fa0bh, 03f751727h
dd	03f75341dh, 03f7550edh, 03f756d97h, 03f758a1ch, 03f75a67bh, 03f75c2b3h
dd	03f75dec6h, 03f75fab3h, 03f76167ah, 03f76321bh, 03f764d97h, 03f7668ech
dd	03f76841bh, 03f769f24h, 03f76ba07h, 03f76d4c4h, 03f76ef5bh, 03f7709cch
dd	03f772417h, 03f773e3ch, 03f77583ah, 03f777213h, 03f778bc5h, 03f77a551h
dd	03f77beb7h, 03f77d7f7h, 03f77f110h, 03f780a04h, 03f7822d1h, 03f783b77h
dd	03f7853f8h, 03f786c52h, 03f788486h, 03f789c93h, 03f78b47bh, 03f78cc3bh
dd	03f78e3d6h, 03f78fb4ah, 03f791298h, 03f7929bfh, 03f7940c0h, 03f79579ah
dd	03f796e4eh, 03f7984dch, 03f799b43h, 03f79b183h, 03f79c79dh, 03f79dd91h
dd	03f79f35eh, 03f7a0904h, 03f7a1e84h, 03f7a33ddh, 03f7a4910h, 03f7a5e1ch
dd	03f7a7302h, 03f7a87c1h, 03f7a9c59h, 03f7ab0cbh, 03f7ac516h, 03f7ad93ah
dd	03f7aed37h, 03f7b010eh, 03f7b14beh, 03f7b2848h, 03f7b3babh, 03f7b4ee7h
dd	03f7b61fch, 03f7b74eah, 03f7b87b2h, 03f7b9a53h, 03f7baccdh, 03f7bbf20h
dd	03f7bd14dh, 03f7be353h, 03f7bf531h, 03f7c06e9h, 03f7c187ah, 03f7c29e5h
dd	03f7c3b28h, 03f7c4c44h, 03f7c5d3ah, 03f7c6e08h, 03f7c7eb0h, 03f7c8f31h
dd	03f7c9f8ah, 03f7cafbdh, 03f7cbfc9h, 03f7ccfaeh, 03f7cdf6ch, 03f7cef03h
dd	03f7cfe73h, 03f7d0dbch, 03f7d1cddh, 03f7d2bd8h, 03f7d3aach, 03f7d4959h
dd	03f7d57deh, 03f7d663dh, 03f7d7474h, 03f7d8285h, 03f7d906eh, 03f7d9e30h
dd	03f7dabcch, 03f7db940h, 03f7dc68ch, 03f7dd3b2h, 03f7de0b1h, 03f7ded88h
dd	03f7dfa38h, 03f7e06c2h, 03f7e1324h, 03f7e1f5eh, 03f7e2b72h, 03f7e375eh
dd	03f7e4323h, 03f7e4ec1h, 03f7e5a38h, 03f7e6588h, 03f7e70b0h, 03f7e7bb1h
dd	03f7e868bh, 03f7e913dh, 03f7e9bc9h, 03f7ea62dh, 03f7eb069h, 03f7eba7fh
dd	03f7ec46dh, 03f7ece34h, 03f7ed7d4h, 03f7ee14ch, 03f7eea9dh, 03f7ef3c7h
dd	03f7efcc9h, 03f7f05a4h, 03f7f0e58h, 03f7f16e4h, 03f7f1f49h, 03f7f2787h
dd	03f7f2f9dh, 03f7f378ch, 03f7f3f54h, 03f7f46f4h, 03f7f4e6dh, 03f7f55bfh
dd	03f7f5ce9h, 03f7f63ech, 03f7f6ac7h, 03f7f717bh, 03f7f7808h, 03f7f7e6dh
dd	03f7f84abh, 03f7f8ac2h, 03f7f90b1h, 03f7f9678h, 03f7f9c18h, 03f7fa191h
dd	03f7fa6e3h, 03f7fac0dh, 03f7fb10fh, 03f7fb5eah, 03f7fba9eh, 03f7fbf2ah
dd	03f7fc38fh, 03f7fc7cch, 03f7fcbe2h, 03f7fcfd1h, 03f7fd397h, 03f7fd737h
dd	03f7fdaafh, 03f7fde00h, 03f7fe129h, 03f7fe42bh, 03f7fe705h, 03f7fe9b8h
dd	03f7fec43h, 03f7feea7h, 03f7ff0e3h, 03f7ff2f8h, 03f7ff4e6h, 03f7ff6ach
dd	03f7ff84ah, 03f7ff9c1h, 03f7ffb11h, 03f7ffc39h, 03f7ffd39h, 03f7ffe13h
dd	03f7ffec4h, 03f7fff4eh, 03f7fffb1h, 03f7fffech

CosTabF	label	DWORD

dd	03f800000h, 03f7fffech, 03f7fffb1h, 03f7fff4eh, 03f7ffec4h, 03f7ffe13h
dd	03f7ffd39h, 03f7ffc39h, 03f7ffb11h, 03f7ff9c1h, 03f7ff84ah, 03f7ff6ach
dd	03f7ff4e6h, 03f7ff2f8h, 03f7ff0e3h, 03f7feea7h, 03f7fec43h, 03f7fe9b8h
dd	03f7fe705h, 03f7fe42bh, 03f7fe129h, 03f7fde00h, 03f7fdaafh, 03f7fd737h
dd	03f7fd397h, 03f7fcfd1h, 03f7fcbe2h, 03f7fc7cch, 03f7fc38fh, 03f7fbf2ah
dd	03f7fba9eh, 03f7fb5eah, 03f7fb10fh, 03f7fac0dh, 03f7fa6e3h, 03f7fa191h
dd	03f7f9c18h, 03f7f9678h, 03f7f90b1h, 03f7f8ac2h, 03f7f84abh, 03f7f7e6dh
dd	03f7f7808h, 03f7f717bh, 03f7f6ac7h, 03f7f63ech, 03f7f5ce9h, 03f7f55bfh
dd	03f7f4e6dh, 03f7f46f4h, 03f7f3f54h, 03f7f378ch, 03f7f2f9dh, 03f7f2787h
dd	03f7f1f49h, 03f7f16e4h, 03f7f0e58h, 03f7f05a4h, 03f7efcc9h, 03f7ef3c7h
dd	03f7eea9dh, 03f7ee14ch, 03f7ed7d4h, 03f7ece34h, 03f7ec46dh, 03f7eba7fh
dd	03f7eb069h, 03f7ea62dh, 03f7e9bc9h, 03f7e913dh, 03f7e868bh, 03f7e7bb1h
dd	03f7e70b0h, 03f7e6588h, 03f7e5a38h, 03f7e4ec1h, 03f7e4323h, 03f7e375eh
dd	03f7e2b72h, 03f7e1f5eh, 03f7e1324h, 03f7e06c2h, 03f7dfa38h, 03f7ded88h
dd	03f7de0b1h, 03f7dd3b2h, 03f7dc68ch, 03f7db940h, 03f7dabcch, 03f7d9e30h
dd	03f7d906eh, 03f7d8285h, 03f7d7474h, 03f7d663dh, 03f7d57deh, 03f7d4959h
dd	03f7d3aach, 03f7d2bd8h, 03f7d1cddh, 03f7d0dbch, 03f7cfe73h, 03f7cef03h
dd	03f7cdf6ch, 03f7ccfaeh, 03f7cbfc9h, 03f7cafbdh, 03f7c9f8ah, 03f7c8f31h
dd	03f7c7eb0h, 03f7c6e08h, 03f7c5d3ah, 03f7c4c44h, 03f7c3b28h, 03f7c29e5h
dd	03f7c187ah, 03f7c06e9h, 03f7bf531h, 03f7be353h, 03f7bd14dh, 03f7bbf20h
dd	03f7baccdh, 03f7b9a53h, 03f7b87b2h, 03f7b74eah, 03f7b61fch, 03f7b4ee7h
dd	03f7b3babh, 03f7b2848h, 03f7b14beh, 03f7b010eh, 03f7aed37h, 03f7ad93ah
dd	03f7ac516h, 03f7ab0cbh, 03f7a9c59h, 03f7a87c1h, 03f7a7302h, 03f7a5e1ch
dd	03f7a4910h, 03f7a33ddh, 03f7a1e84h, 03f7a0904h, 03f79f35eh, 03f79dd91h
dd	03f79c79dh, 03f79b183h, 03f799b43h, 03f7984dch, 03f796e4eh, 03f79579ah
dd	03f7940c0h, 03f7929bfh, 03f791298h, 03f78fb4ah, 03f78e3d6h, 03f78cc3bh
dd	03f78b47bh, 03f789c93h, 03f788486h, 03f786c52h, 03f7853f8h, 03f783b77h
dd	03f7822d1h, 03f780a04h, 03f77f110h, 03f77d7f7h, 03f77beb7h, 03f77a551h
dd	03f778bc5h, 03f777213h, 03f77583ah, 03f773e3ch, 03f772417h, 03f7709cch
dd	03f76ef5bh, 03f76d4c4h, 03f76ba07h, 03f769f24h, 03f76841bh, 03f7668ech
dd	03f764d97h, 03f76321bh, 03f76167ah, 03f75fab3h, 03f75dec6h, 03f75c2b3h
dd	03f75a67bh, 03f758a1ch, 03f756d97h, 03f7550edh, 03f75341dh, 03f751727h
dd	03f74fa0bh, 03f74dcc9h, 03f74bf62h, 03f74a1d5h, 03f748422h, 03f746649h
dd	03f74484bh, 03f742a27h, 03f740bddh, 03f73ed6eh, 03f73ced9h, 03f73b01fh
dd	03f73913fh, 03f737239h, 03f73530eh, 03f7333beh, 03f731447h, 03f72f4ach
dd	03f72d4ebh, 03f72b504h, 03f7294f8h, 03f7274c7h, 03f725470h, 03f7233f4h
dd	03f721352h, 03f71f28ch, 03f71d19fh, 03f71b08eh, 03f718f57h, 03f716dfbh
dd	03f714c7ah, 03f712ad4h, 03f710908h, 03f70e717h, 03f70c501h, 03f70a2c6h
dd	03f708066h, 03f705de1h, 03f703b37h, 03f701867h, 03f6ff573h, 03f6fd25ah
dd	03f6faf1bh, 03f6f8bb8h, 03f6f6830h, 03f6f4483h, 03f6f20b0h, 03f6efcbah
dd	03f6ed89eh, 03f6eb45dh, 03f6e8ff8h, 03f6e6b6dh, 03f6e46beh, 03f6e21ebh
dd	03f6dfcf2h, 03f6dd7d5h, 03f6db293h, 03f6d8d2dh, 03f6d67a1h, 03f6d41f2h
dd	03f6d1c1dh, 03f6cf624h, 03f6cd007h, 03f6ca9c5h, 03f6c835eh, 03f6c5cd4h
dd	03f6c3624h, 03f6c0f50h, 03f6be858h, 03f6bc13bh, 03f6b99fbh, 03f6b7295h
dd	03f6b4b0ch, 03f6b235eh, 03f6afb8ch, 03f6ad395h, 03f6aab7bh, 03f6a833ch
dd	03f6a5ad9h, 03f6a3252h, 03f6a09a7h, 03f69e0d7h, 03f69b7e4h, 03f698ecch
dd	03f696591h, 03f693c32h, 03f6912aeh, 03f68e907h, 03f68bf3ch, 03f68954ch
dd	03f686b39h, 03f684103h, 03f6816a8h, 03f67ec29h, 03f67c187h, 03f6796c1h
dd	03f676bd8h, 03f6740cah, 03f671599h, 03f66ea45h, 03f66becch, 03f669330h
dd	03f666771h, 03f663b8eh, 03f660f88h, 03f65e35eh, 03f65b710h, 03f658aa0h
dd	03f655e0bh, 03f653154h, 03f650479h, 03f64d77bh, 03f64aa59h, 03f647d14h
dd	03f644fach, 03f642221h, 03f63f473h, 03f63c6a1h, 03f6398ach, 03f636a95h
dd	03f633c5ah, 03f630dfch, 03f62df7bh, 03f62b0d7h, 03f628210h, 03f625326h
dd	03f62241ah, 03f61f4eah, 03f61c598h, 03f619622h, 03f61668ah, 03f6136d0h
dd	03f6106f2h, 03f60d6f2h, 03f60a6cfh, 03f607689h, 03f604621h, 03f601596h
dd	03f5fe4e9h, 03f5fb419h, 03f5f8327h, 03f5f5212h, 03f5f20dbh, 03f5eef81h
dd	03f5ebe05h, 03f5e8c67h, 03f5e5aa6h, 03f5e28c3h, 03f5df6beh, 03f5dc497h
dd	03f5d924dh, 03f5d5fe1h, 03f5d2d53h, 03f5cfaa3h, 03f5cc7d1h, 03f5c94ddh
dd	03f5c61c7h, 03f5c2e8eh, 03f5bfb34h, 03f5bc7b8h, 03f5b941ah, 03f5b605ah
dd	03f5b2c79h, 03f5af875h, 03f5ac450h, 03f5a9009h, 03f5a5ba0h, 03f5a2716h
dd	03f59f26ah, 03f59bd9ch, 03f5988adh, 03f59539ch, 03f591e6ah, 03f58e916h
dd	03f58b3a1h, 03f587e0bh, 03f584853h, 03f581279h, 03f57dc7fh, 03f57a663h
dd	03f577026h, 03f5739c7h, 03f570348h, 03f56cca7h, 03f5695e5h, 03f565f02h
dd	03f5627feh, 03f55f0d9h, 03f55b993h, 03f55822ch, 03f554aa4h, 03f5512fbh
dd	03f54db31h, 03f54a347h, 03f546b3bh, 03f54330fh, 03f53fac3h, 03f53c255h
dd	03f5389c7h, 03f535118h, 03f531849h, 03f52df59h, 03f52a649h, 03f526d18h
dd	03f5233c6h, 03f51fa54h, 03f51c0c2h, 03f518710h, 03f514d3dh, 03f51134ah
dd	03f50d937h, 03f509f03h, 03f5064afh, 03f502a3bh, 03f4fefa8h, 03f4fb4f4h
dd	03f4f7a1fh, 03f4f3f2bh, 03f4f0417h, 03f4ec8e4h, 03f4e8d90h, 03f4e521ch
dd	03f4e1689h, 03f4ddad5h, 03f4d9f02h, 03f4d6310h, 03f4d26fdh, 03f4ceacbh
dd	03f4cae79h, 03f4c7208h, 03f4c3578h, 03f4bf8c7h, 03f4bbbf8h, 03f4b7f09h
dd	03f4b41fah, 03f4b04cch, 03f4ac77fh, 03f4a8a13h, 03f4a4c87h, 03f4a0edch
dd	03f49d112h, 03f499329h, 03f495521h, 03f4916fah, 03f48d8b3h, 03f489a4eh
dd	03f485bcah, 03f481d27h, 03f47de65h, 03f479f84h, 03f476085h, 03f472167h
dd	03f46e22ah, 03f46a2ceh, 03f466354h, 03f4623bbh, 03f45e403h, 03f45a42dh
dd	03f456439h, 03f452426h, 03f44e3f5h, 03f44a3a5h, 03f446337h, 03f4422abh
dd	03f43e200h, 03f43a138h, 03f436051h, 03f431f4ch, 03f42de29h, 03f429ce8h
dd	03f425b89h, 03f421a0bh, 03f41d870h, 03f4196b7h, 03f4154e1h, 03f4112ech
dd	03f40d0dah, 03f408ea9h, 03f404c5ch, 03f4009f0h, 03f3fc767h, 03f3f84c0h
dd	03f3f41fch, 03f3eff1bh, 03f3ebc1bh, 03f3e78ffh, 03f3e35c5h, 03f3df26eh
dd	03f3daef9h, 03f3d6b67h, 03f3d27b8h, 03f3ce3ech, 03f3ca003h, 03f3c5bfch
dd	03f3c17d9h, 03f3bd398h, 03f3b8f3bh, 03f3b4ac1h, 03f3b0629h, 03f3ac175h
dd	03f3a7ca4h, 03f3a37b7h, 03f39f2ach, 03f39ad85h, 03f396842h, 03f3922e1h
dd	03f38dd65h, 03f3897cbh, 03f385216h, 03f380c43h, 03f37c655h, 03f37804ah
dd	03f373a23h, 03f36f3dfh, 03f36ad7fh, 03f366704h, 03f36206ch, 03f35d9b8h
dd	03f3592e7h, 03f354bfbh, 03f3504f3h, 03f34bdcfh, 03f34768fh, 03f342f34h
dd	03f33e7bch, 03f33a029h, 03f33587ah, 03f3310afh, 03f32c8c9h, 03f3280c7h
dd	03f3238aah, 03f31f071h, 03f31a81dh, 03f315fadh, 03f311722h, 03f30ce7ch
dd	03f3085bbh, 03f303cdeh, 03f2ff3e6h, 03f2faad3h, 03f2f61a5h, 03f2f185bh
dd	03f2ecef7h, 03f2e8578h, 03f2e3bdeh, 03f2df229h, 03f2da859h, 03f2d5e6fh
dd	03f2d1469h, 03f2cca49h, 03f2c800fh, 03f2c35b9h, 03f2beb4ah, 03f2ba0bfh
dd	03f2b561bh, 03f2b0b5bh, 03f2ac082h, 03f2a758eh, 03f2a2a80h, 03f29df57h
dd	03f299415h, 03f2948b8h, 03f28fd41h, 03f28b1b0h, 03f286605h, 03f281a40h
dd	03f27ce61h, 03f278268h, 03f273656h, 03f26ea2ah, 03f269de3h, 03f265184h
dd	03f26050ah, 03f25b877h, 03f256bcbh, 03f251f04h, 03f24d225h, 03f24852ch
dd	03f24381ah, 03f23eaeeh, 03f239da9h, 03f23504bh, 03f2302d3h, 03f22b543h
dd	03f226799h, 03f2219d7h, 03f21cbfbh, 03f217e06h, 03f212ff9h, 03f20e1d2h
dd	03f209393h, 03f20453bh, 03f1ff6cbh, 03f1fa841h, 03f1f599fh, 03f1f0ae5h
dd	03f1ebc12h, 03f1e6d26h, 03f1e1e22h, 03f1dcf06h, 03f1d7fd1h, 03f1d3084h
dd	03f1ce11fh, 03f1c91a2h, 03f1c420ch, 03f1bf25fh, 03f1ba299h, 03f1b52bbh
dd	03f1b02c6h, 03f1ab2b8h, 03f1a6293h, 03f1a1255h, 03f19c200h, 03f197194h
dd	03f19210fh, 03f18d073h, 03f187fc0h, 03f182ef5h, 03f17de12h, 03f178d18h
dd	03f173c07h, 03f16eadeh, 03f16999fh, 03f164847h, 03f15f6d9h, 03f15a554h
dd	03f1553b7h, 03f150204h, 03f14b039h, 03f145e58h, 03f140c5fh, 03f13ba50h
dd	03f13682ah, 03f1315eeh, 03f12c39ah, 03f127130h, 03f121eb0h, 03f11cc19h
dd	03f11796bh, 03f1126a7h, 03f10d3cdh, 03f1080dch, 03f102dd5h, 03f0fdab8h
dd	03f0f8784h, 03f0f343bh, 03f0ee0dbh, 03f0e8d65h, 03f0e39dah, 03f0de638h
dd	03f0d9281h, 03f0d3eb3h, 03f0cead0h, 03f0c96d7h, 03f0c42c9h, 03f0beea5h
dd	03f0b9a6bh, 03f0b461ch, 03f0af1b7h, 03f0a9d3dh, 03f0a48adh, 03f09f409h
dd	03f099f4eh, 03f094a7fh, 03f08f59bh, 03f08a0a1h, 03f084b92h, 03f07f66fh
dd	03f07a136h, 03f074be8h, 03f06f686h, 03f06a10eh, 03f064b82h, 03f05f5e2h
dd	03f05a02ch, 03f054a62h, 03f04f484h, 03f049e91h, 03f044889h, 03f03f26dh
dd	03f039c3dh, 03f0345f8h, 03f02ef9fh, 03f029932h, 03f0242b1h, 03f01ec1ch
dd	03f019573h, 03f013eb5h, 03f00e7e4h, 03f0090ffh, 03f003a06h, 03effc5f3h
dd	03eff17b2h, 03efe694ah, 03efdbabbh, 03efd0c04h, 03efc5d27h, 03efbae22h
dd	03efafef7h, 03efa4fa5h, 03ef9a02dh, 03ef8f08eh, 03ef840c8h, 03ef790dch
dd	03ef6e0cbh, 03ef63093h, 03ef58035h, 03ef4cfb1h, 03ef41f07h, 03ef36e38h
dd	03ef2bd43h, 03ef20c29h, 03ef15aeah, 03ef0a985h, 03eeff7fbh, 03eef464ch
dd	03eee9479h, 03eede280h, 03eed3063h, 03eec7e21h, 03eebcbbbh, 03eeb1930h
dd	03eea6681h, 03ee9b3aeh, 03ee900b7h, 03ee84d9ch, 03ee79a5dh, 03ee6e6fbh
dd	03ee63375h, 03ee57fcbh, 03ee4cbfeh, 03ee4180eh, 03ee363fah, 03ee2afc4h
dd	03ee1fb6ah, 03ee146eeh, 03ee0924fh, 03edfdd8dh, 03edf28a9h, 03ede73a2h
dd	03eddbe79h, 03edd092eh, 03edc53c1h, 03edb9e31h, 03edae880h, 03eda32adh
dd	03ed97cb9h, 03ed8c6a3h, 03ed8106bh, 03ed75a13h, 03ed6a399h, 03ed5ecfdh
dd	03ed53641h, 03ed47f64h, 03ed3c867h, 03ed31148h, 03ed25a09h, 03ed1a2aah
dd	03ed0eb2ah, 03ed0338ah, 03ecf7bcah, 03ecec3eah, 03ece0beah, 03ecd53cah
dd	03ecc9b8bh, 03ecbe32ch, 03ecb2aaeh, 03eca7210h, 03ec9b953h, 03ec90077h
dd	03ec8477ch, 03ec78e62h, 03ec6d529h, 03ec61bd2h, 03ec5625ch, 03ec4a8c8h
dd	03ec3ef15h, 03ec33544h, 03ec27b55h, 03ec1c148h, 03ec1071eh, 03ec04cd5h
dd	03ebf926fh, 03ebed7ebh, 03ebe1d4ah, 03ebd628bh, 03ebca7afh, 03ebbecb6h
dd	03ebb31a0h, 03eba766eh, 03eb9bb1eh, 03eb8ffb2h, 03eb8442ah, 03eb78884h
dd	03eb6ccc3h, 03eb610e6h, 03eb554ech, 03eb498d6h, 03eb3dca5h, 03eb32058h
dd	03eb263efh, 03eb1a76bh, 03eb0eacbh, 03eb02e10h, 03eaf713ah, 03eaeb449h
dd	03eadf73ch, 03ead3a15h, 03eac7cd4h, 03eabbf77h, 03eab0201h, 03eaa446fh
dd	03ea986c4h, 03ea8c8feh, 03ea80b1fh, 03ea74d25h, 03ea68f12h, 03ea5d0e5h
dd	03ea5129fh, 03ea4543fh, 03ea395c5h, 03ea2d733h, 03ea21887h, 03ea159c2h
dd	03ea09ae5h, 03e9fdbeeh, 03e9f1cdfh, 03e9e5db8h, 03e9d9e78h, 03e9cdf20h
dd	03e9c1fafh, 03e9b6027h, 03e9aa086h, 03e99e0ceh, 03e9920feh, 03e986116h
dd	03e97a117h, 03e96e100h, 03e9620d2h, 03e95608dh, 03e94a031h, 03e93dfbfh
dd	03e931f35h, 03e925e94h, 03e919dddh, 03e90dd10h, 03e901c2ch, 03e8f5b32h
dd	03e8e9a22h, 03e8dd8fch, 03e8d17c0h, 03e8c566eh, 03e8b9507h, 03e8ad38ah
dd	03e8a11f7h, 03e895050h, 03e888e93h, 03e87ccc1h, 03e870adah, 03e8648dfh
dd	03e8586ceh, 03e84c4aah, 03e840270h, 03e834022h, 03e827dc0h, 03e81bb4ah
dd	03e80f8c0h, 03e803622h, 03e7ee6e1h, 03e7d6156h, 03e7bdba4h, 03e7a55cbh
dd	03e78cfcch, 03e7749a6h, 03e75c35ah, 03e743ce8h, 03e72b651h, 03e712f94h
dd	03e6fa8b2h, 03e6e21abh, 03e6c9a7fh, 03e6b132fh, 03e698bbah, 03e680422h
dd	03e667c66h, 03e64f486h, 03e636c83h, 03e61e45ch, 03e605c13h, 03e5ed3a8h
dd	03e5d4b1ah, 03e5bc26ah, 03e5a3997h, 03e58b0a4h, 03e57278fh, 03e559e58h
dd	03e541501h, 03e528b89h, 03e5101f1h, 03e4f7838h, 03e4dee60h, 03e4c6467h
dd	03e4ada4fh, 03e495018h, 03e47c5c2h, 03e463b4dh, 03e44b0b9h, 03e432607h
dd	03e419b37h, 03e401049h, 03e3e853eh, 03e3cfa15h, 03e3b6ecfh, 03e39e36ch
dd	03e3857ech, 03e36cc50h, 03e354098h, 03e33b4c4h, 03e3228d4h, 03e309cc9h
dd	03e2f10a2h, 03e2d8461h, 03e2bf804h, 03e2a6b8dh, 03e28defch, 03e275251h
dd	03e25c58ch, 03e2438adh, 03e22abb6h, 03e211ea5h, 03e1f917bh, 03e1e0438h
dd	03e1c76deh, 03e1ae96bh, 03e195be0h, 03e17ce3dh, 03e164083h, 03e14b2b2h
dd	03e1324cah, 03e1196cch, 03e1008b7h, 03e0e7a8bh, 03e0cec4ah, 03e0b5df3h
dd	03e09cf86h, 03e084105h, 03e06b26eh, 03e0523c2h, 03e039502h, 03e02062eh
dd	03e007745h, 03dfdd092h, 03dfab273h, 03df7942ch, 03df475c0h, 03df1572eh
dd	03dee3876h, 03deb199ah, 03de7fa9ah, 03de4db76h, 03de1bc2eh, 03dde9cc4h
dd	03ddb7d37h, 03dd85d89h, 03dd53db9h, 03dd21dc8h, 03dcefdb7h, 03dcbdd86h
dd	03dc8bd36h, 03dc59cc6h, 03dc27c39h, 03dbf5b8dh, 03dbc3ac3h, 03db919ddh
dd	03db5f8dah, 03db2d7bbh, 03dafb680h, 03dac952bh, 03da973bah, 03da65230h
dd	03da3308ch, 03da00ecfh, 03d9cecf9h, 03d99cb0ah, 03d96a905h, 03d9386e7h
dd	03d9064b4h, 03d8d426ah, 03d8a200ah, 03d86fd94h, 03d83db0ah, 03d80b86ch
dd	03d7b2b74h, 03d74e5e9h, 03d6ea038h, 03d685a62h, 03d621469h, 03d5bce4ch
dd	03d55880eh, 03d4f41afh, 03d48fb30h, 03d42b491h, 03d3c6dd5h, 03d3626fch
dd	03d2fe007h, 03d2998f6h, 03d2351cbh, 03d1d0a88h, 03d16c32ch, 03d107bb8h
dd	03d0a342fh, 03d03ec90h, 03cfb49bah, 03ceeba2ch, 03ce22a7ah, 03cd59aa6h
dd	03cc90ab0h, 03cbc7a9bh, 03cafea69h, 03ca35a1ch, 03c96c9b6h, 03c8a3938h
dd	03c7b514bh, 03c622fffh, 03c490e90h, 03c2fed02h, 03c16cb58h, 03bfb5330h
dd	03bc90f88h, 03b96cbc1h, 03b490fc6h, 03ac90fd5h, 000000000h, 0bac90fd5h
dd	0bb490fc6h, 0bb96cbc1h, 0bbc90f88h, 0bbfb5330h, 0bc16cb58h, 0bc2fed02h
dd	0bc490e90h, 0bc622fffh, 0bc7b514bh, 0bc8a3938h, 0bc96c9b6h, 0bca35a1ch
dd	0bcafea69h, 0bcbc7a9bh, 0bcc90ab0h, 0bcd59aa6h, 0bce22a7ah, 0bceeba2ch
dd	0bcfb49bah, 0bd03ec90h, 0bd0a342fh, 0bd107bb8h, 0bd16c32ch, 0bd1d0a88h
dd	0bd2351cbh, 0bd2998f6h, 0bd2fe007h, 0bd3626fch, 0bd3c6dd5h, 0bd42b491h
dd	0bd48fb30h, 0bd4f41afh, 0bd55880eh, 0bd5bce4ch, 0bd621469h, 0bd685a62h
dd	0bd6ea038h, 0bd74e5e9h, 0bd7b2b74h, 0bd80b86ch, 0bd83db0ah, 0bd86fd94h
dd	0bd8a200ah, 0bd8d426ah, 0bd9064b4h, 0bd9386e7h, 0bd96a905h, 0bd99cb0ah
dd	0bd9cecf9h, 0bda00ecfh, 0bda3308ch, 0bda65230h, 0bda973bah, 0bdac952bh
dd	0bdafb680h, 0bdb2d7bbh, 0bdb5f8dah, 0bdb919ddh, 0bdbc3ac3h, 0bdbf5b8dh
dd	0bdc27c39h, 0bdc59cc6h, 0bdc8bd36h, 0bdcbdd86h, 0bdcefdb7h, 0bdd21dc8h
dd	0bdd53db9h, 0bdd85d89h, 0bddb7d37h, 0bdde9cc4h, 0bde1bc2eh, 0bde4db76h
dd	0bde7fa9ah, 0bdeb199ah, 0bdee3876h, 0bdf1572eh, 0bdf475c0h, 0bdf7942ch
dd	0bdfab273h, 0bdfdd092h, 0be007745h, 0be02062eh, 0be039502h, 0be0523c2h
dd	0be06b26eh, 0be084105h, 0be09cf86h, 0be0b5df3h, 0be0cec4ah, 0be0e7a8bh
dd	0be1008b7h, 0be1196cch, 0be1324cah, 0be14b2b2h, 0be164083h, 0be17ce3dh
dd	0be195be0h, 0be1ae96bh, 0be1c76deh, 0be1e0438h, 0be1f917bh, 0be211ea5h
dd	0be22abb6h, 0be2438adh, 0be25c58ch, 0be275251h, 0be28defch, 0be2a6b8dh
dd	0be2bf804h, 0be2d8461h, 0be2f10a2h, 0be309cc9h, 0be3228d4h, 0be33b4c4h
dd	0be354098h, 0be36cc50h, 0be3857ech, 0be39e36ch, 0be3b6ecfh, 0be3cfa15h
dd	0be3e853eh, 0be401049h, 0be419b37h, 0be432607h, 0be44b0b9h, 0be463b4dh
dd	0be47c5c2h, 0be495018h, 0be4ada4fh, 0be4c6467h, 0be4dee60h, 0be4f7838h
dd	0be5101f1h, 0be528b89h, 0be541501h, 0be559e58h, 0be57278fh, 0be58b0a4h
dd	0be5a3997h, 0be5bc26ah, 0be5d4b1ah, 0be5ed3a8h, 0be605c13h, 0be61e45ch
dd	0be636c83h, 0be64f486h, 0be667c66h, 0be680422h, 0be698bbah, 0be6b132fh
dd	0be6c9a7fh, 0be6e21abh, 0be6fa8b2h, 0be712f94h, 0be72b651h, 0be743ce8h
dd	0be75c35ah, 0be7749a6h, 0be78cfcch, 0be7a55cbh, 0be7bdba4h, 0be7d6156h
dd	0be7ee6e1h, 0be803622h, 0be80f8c0h, 0be81bb4ah, 0be827dc0h, 0be834022h
dd	0be840270h, 0be84c4aah, 0be8586ceh, 0be8648dfh, 0be870adah, 0be87ccc1h
dd	0be888e93h, 0be895050h, 0be8a11f7h, 0be8ad38ah, 0be8b9507h, 0be8c566eh
dd	0be8d17c0h, 0be8dd8fch, 0be8e9a22h, 0be8f5b32h, 0be901c2ch, 0be90dd10h
dd	0be919dddh, 0be925e94h, 0be931f35h, 0be93dfbfh, 0be94a031h, 0be95608dh
dd	0be9620d2h, 0be96e100h, 0be97a117h, 0be986116h, 0be9920feh, 0be99e0ceh
dd	0be9aa086h, 0be9b6027h, 0be9c1fafh, 0be9cdf20h, 0be9d9e78h, 0be9e5db8h
dd	0be9f1cdfh, 0be9fdbeeh, 0bea09ae5h, 0bea159c2h, 0bea21887h, 0bea2d733h
dd	0bea395c5h, 0bea4543fh, 0bea5129fh, 0bea5d0e5h, 0bea68f12h, 0bea74d25h
dd	0bea80b1fh, 0bea8c8feh, 0bea986c4h, 0beaa446fh, 0beab0201h, 0beabbf77h
dd	0beac7cd4h, 0bead3a15h, 0beadf73ch, 0beaeb449h, 0beaf713ah, 0beb02e10h
dd	0beb0eacbh, 0beb1a76bh, 0beb263efh, 0beb32058h, 0beb3dca5h, 0beb498d6h
dd	0beb554ech, 0beb610e6h, 0beb6ccc3h, 0beb78884h, 0beb8442ah, 0beb8ffb2h
dd	0beb9bb1eh, 0beba766eh, 0bebb31a0h, 0bebbecb6h, 0bebca7afh, 0bebd628bh
dd	0bebe1d4ah, 0bebed7ebh, 0bebf926fh, 0bec04cd5h, 0bec1071eh, 0bec1c148h
dd	0bec27b55h, 0bec33544h, 0bec3ef15h, 0bec4a8c8h, 0bec5625ch, 0bec61bd2h
dd	0bec6d529h, 0bec78e62h, 0bec8477ch, 0bec90077h, 0bec9b953h, 0beca7210h
dd	0becb2aaeh, 0becbe32ch, 0becc9b8bh, 0becd53cah, 0bece0beah, 0becec3eah
dd	0becf7bcah, 0bed0338ah, 0bed0eb2ah, 0bed1a2aah, 0bed25a09h, 0bed31148h
dd	0bed3c867h, 0bed47f64h, 0bed53641h, 0bed5ecfdh, 0bed6a399h, 0bed75a13h
dd	0bed8106bh, 0bed8c6a3h, 0bed97cb9h, 0beda32adh, 0bedae880h, 0bedb9e31h
dd	0bedc53c1h, 0bedd092eh, 0beddbe79h, 0bede73a2h, 0bedf28a9h, 0bedfdd8dh
dd	0bee0924fh, 0bee146eeh, 0bee1fb6ah, 0bee2afc4h, 0bee363fah, 0bee4180eh
dd	0bee4cbfeh, 0bee57fcbh, 0bee63375h, 0bee6e6fbh, 0bee79a5dh, 0bee84d9ch
dd	0bee900b7h, 0bee9b3aeh, 0beea6681h, 0beeb1930h, 0beebcbbbh, 0beec7e21h
dd	0beed3063h, 0beede280h, 0beee9479h, 0beef464ch, 0beeff7fbh, 0bef0a985h
dd	0bef15aeah, 0bef20c29h, 0bef2bd43h, 0bef36e38h, 0bef41f07h, 0bef4cfb1h
dd	0bef58035h, 0bef63093h, 0bef6e0cbh, 0bef790dch, 0bef840c8h, 0bef8f08eh
dd	0bef9a02dh, 0befa4fa5h, 0befafef7h, 0befbae22h, 0befc5d27h, 0befd0c04h
dd	0befdbabbh, 0befe694ah, 0beff17b2h, 0beffc5f3h, 0bf003a06h, 0bf0090ffh
dd	0bf00e7e4h, 0bf013eb5h, 0bf019573h, 0bf01ec1ch, 0bf0242b1h, 0bf029932h
dd	0bf02ef9fh, 0bf0345f8h, 0bf039c3dh, 0bf03f26dh, 0bf044889h, 0bf049e91h
dd	0bf04f484h, 0bf054a62h, 0bf05a02ch, 0bf05f5e2h, 0bf064b82h, 0bf06a10eh
dd	0bf06f686h, 0bf074be8h, 0bf07a136h, 0bf07f66fh, 0bf084b92h, 0bf08a0a1h
dd	0bf08f59bh, 0bf094a7fh, 0bf099f4eh, 0bf09f409h, 0bf0a48adh, 0bf0a9d3dh
dd	0bf0af1b7h, 0bf0b461ch, 0bf0b9a6bh, 0bf0beea5h, 0bf0c42c9h, 0bf0c96d7h
dd	0bf0cead0h, 0bf0d3eb3h, 0bf0d9281h, 0bf0de638h, 0bf0e39dah, 0bf0e8d65h
dd	0bf0ee0dbh, 0bf0f343bh, 0bf0f8784h, 0bf0fdab8h, 0bf102dd5h, 0bf1080dch
dd	0bf10d3cdh, 0bf1126a7h, 0bf11796bh, 0bf11cc19h, 0bf121eb0h, 0bf127130h
dd	0bf12c39ah, 0bf1315eeh, 0bf13682ah, 0bf13ba50h, 0bf140c5fh, 0bf145e58h
dd	0bf14b039h, 0bf150204h, 0bf1553b7h, 0bf15a554h, 0bf15f6d9h, 0bf164847h
dd	0bf16999fh, 0bf16eadeh, 0bf173c07h, 0bf178d18h, 0bf17de12h, 0bf182ef5h
dd	0bf187fc0h, 0bf18d073h, 0bf19210fh, 0bf197194h, 0bf19c200h, 0bf1a1255h
dd	0bf1a6293h, 0bf1ab2b8h, 0bf1b02c6h, 0bf1b52bbh, 0bf1ba299h, 0bf1bf25fh
dd	0bf1c420ch, 0bf1c91a2h, 0bf1ce11fh, 0bf1d3084h, 0bf1d7fd1h, 0bf1dcf06h
dd	0bf1e1e22h, 0bf1e6d26h, 0bf1ebc12h, 0bf1f0ae5h, 0bf1f599fh, 0bf1fa841h
dd	0bf1ff6cbh, 0bf20453bh, 0bf209393h, 0bf20e1d2h, 0bf212ff9h, 0bf217e06h
dd	0bf21cbfbh, 0bf2219d7h, 0bf226799h, 0bf22b543h, 0bf2302d3h, 0bf23504bh
dd	0bf239da9h, 0bf23eaeeh, 0bf24381ah, 0bf24852ch, 0bf24d225h, 0bf251f04h
dd	0bf256bcbh, 0bf25b877h, 0bf26050ah, 0bf265184h, 0bf269de3h, 0bf26ea2ah
dd	0bf273656h, 0bf278268h, 0bf27ce61h, 0bf281a40h, 0bf286605h, 0bf28b1b0h
dd	0bf28fd41h, 0bf2948b8h, 0bf299415h, 0bf29df57h, 0bf2a2a80h, 0bf2a758eh
dd	0bf2ac082h, 0bf2b0b5bh, 0bf2b561bh, 0bf2ba0bfh, 0bf2beb4ah, 0bf2c35b9h
dd	0bf2c800fh, 0bf2cca49h, 0bf2d1469h, 0bf2d5e6fh, 0bf2da859h, 0bf2df229h
dd	0bf2e3bdeh, 0bf2e8578h, 0bf2ecef7h, 0bf2f185bh, 0bf2f61a5h, 0bf2faad3h
dd	0bf2ff3e6h, 0bf303cdeh, 0bf3085bbh, 0bf30ce7ch, 0bf311722h, 0bf315fadh
dd	0bf31a81dh, 0bf31f071h, 0bf3238aah, 0bf3280c7h, 0bf32c8c9h, 0bf3310afh
dd	0bf33587ah, 0bf33a029h, 0bf33e7bch, 0bf342f34h, 0bf34768fh, 0bf34bdcfh
dd	0bf3504f3h, 0bf354bfbh, 0bf3592e7h, 0bf35d9b8h, 0bf36206ch, 0bf366704h
dd	0bf36ad7fh, 0bf36f3dfh, 0bf373a23h, 0bf37804ah, 0bf37c655h, 0bf380c43h
dd	0bf385216h, 0bf3897cbh, 0bf38dd65h, 0bf3922e1h, 0bf396842h, 0bf39ad85h
dd	0bf39f2ach, 0bf3a37b7h, 0bf3a7ca4h, 0bf3ac175h, 0bf3b0629h, 0bf3b4ac1h
dd	0bf3b8f3bh, 0bf3bd398h, 0bf3c17d9h, 0bf3c5bfch, 0bf3ca003h, 0bf3ce3ech
dd	0bf3d27b8h, 0bf3d6b67h, 0bf3daef9h, 0bf3df26eh, 0bf3e35c5h, 0bf3e78ffh
dd	0bf3ebc1bh, 0bf3eff1bh, 0bf3f41fch, 0bf3f84c0h, 0bf3fc767h, 0bf4009f0h
dd	0bf404c5ch, 0bf408ea9h, 0bf40d0dah, 0bf4112ech, 0bf4154e1h, 0bf4196b7h
dd	0bf41d870h, 0bf421a0bh, 0bf425b89h, 0bf429ce8h, 0bf42de29h, 0bf431f4ch
dd	0bf436051h, 0bf43a138h, 0bf43e200h, 0bf4422abh, 0bf446337h, 0bf44a3a5h
dd	0bf44e3f5h, 0bf452426h, 0bf456439h, 0bf45a42dh, 0bf45e403h, 0bf4623bbh
dd	0bf466354h, 0bf46a2ceh, 0bf46e22ah, 0bf472167h, 0bf476085h, 0bf479f84h
dd	0bf47de65h, 0bf481d27h, 0bf485bcah, 0bf489a4eh, 0bf48d8b3h, 0bf4916fah
dd	0bf495521h, 0bf499329h, 0bf49d112h, 0bf4a0edch, 0bf4a4c87h, 0bf4a8a13h
dd	0bf4ac77fh, 0bf4b04cch, 0bf4b41fah, 0bf4b7f09h, 0bf4bbbf8h, 0bf4bf8c7h
dd	0bf4c3578h, 0bf4c7208h, 0bf4cae79h, 0bf4ceacbh, 0bf4d26fdh, 0bf4d6310h
dd	0bf4d9f02h, 0bf4ddad5h, 0bf4e1689h, 0bf4e521ch, 0bf4e8d90h, 0bf4ec8e4h
dd	0bf4f0417h, 0bf4f3f2bh, 0bf4f7a1fh, 0bf4fb4f4h, 0bf4fefa8h, 0bf502a3bh
dd	0bf5064afh, 0bf509f03h, 0bf50d937h, 0bf51134ah, 0bf514d3dh, 0bf518710h
dd	0bf51c0c2h, 0bf51fa54h, 0bf5233c6h, 0bf526d18h, 0bf52a649h, 0bf52df59h
dd	0bf531849h, 0bf535118h, 0bf5389c7h, 0bf53c255h, 0bf53fac3h, 0bf54330fh
dd	0bf546b3bh, 0bf54a347h, 0bf54db31h, 0bf5512fbh, 0bf554aa4h, 0bf55822ch
dd	0bf55b993h, 0bf55f0d9h, 0bf5627feh, 0bf565f02h, 0bf5695e5h, 0bf56cca7h
dd	0bf570348h, 0bf5739c7h, 0bf577026h, 0bf57a663h, 0bf57dc7fh, 0bf581279h
dd	0bf584853h, 0bf587e0bh, 0bf58b3a1h, 0bf58e916h, 0bf591e6ah, 0bf59539ch
dd	0bf5988adh, 0bf59bd9ch, 0bf59f26ah, 0bf5a2716h, 0bf5a5ba0h, 0bf5a9009h
dd	0bf5ac450h, 0bf5af875h, 0bf5b2c79h, 0bf5b605ah, 0bf5b941ah, 0bf5bc7b8h
dd	0bf5bfb34h, 0bf5c2e8eh, 0bf5c61c7h, 0bf5c94ddh, 0bf5cc7d1h, 0bf5cfaa3h
dd	0bf5d2d53h, 0bf5d5fe1h, 0bf5d924dh, 0bf5dc497h, 0bf5df6beh, 0bf5e28c3h
dd	0bf5e5aa6h, 0bf5e8c67h, 0bf5ebe05h, 0bf5eef81h, 0bf5f20dbh, 0bf5f5212h
dd	0bf5f8327h, 0bf5fb419h, 0bf5fe4e9h, 0bf601596h, 0bf604621h, 0bf607689h
dd	0bf60a6cfh, 0bf60d6f2h, 0bf6106f2h, 0bf6136d0h, 0bf61668ah, 0bf619622h
dd	0bf61c598h, 0bf61f4eah, 0bf62241ah, 0bf625326h, 0bf628210h, 0bf62b0d7h
dd	0bf62df7bh, 0bf630dfch, 0bf633c5ah, 0bf636a95h, 0bf6398ach, 0bf63c6a1h
dd	0bf63f473h, 0bf642221h, 0bf644fach, 0bf647d14h, 0bf64aa59h, 0bf64d77bh
dd	0bf650479h, 0bf653154h, 0bf655e0bh, 0bf658aa0h, 0bf65b710h, 0bf65e35eh
dd	0bf660f88h, 0bf663b8eh, 0bf666771h, 0bf669330h, 0bf66becch, 0bf66ea45h
dd	0bf671599h, 0bf6740cah, 0bf676bd8h, 0bf6796c1h, 0bf67c187h, 0bf67ec29h
dd	0bf6816a8h, 0bf684103h, 0bf686b39h, 0bf68954ch, 0bf68bf3ch, 0bf68e907h
dd	0bf6912aeh, 0bf693c32h, 0bf696591h, 0bf698ecch, 0bf69b7e4h, 0bf69e0d7h
dd	0bf6a09a7h, 0bf6a3252h, 0bf6a5ad9h, 0bf6a833ch, 0bf6aab7bh, 0bf6ad395h
dd	0bf6afb8ch, 0bf6b235eh, 0bf6b4b0ch, 0bf6b7295h, 0bf6b99fbh, 0bf6bc13bh
dd	0bf6be858h, 0bf6c0f50h, 0bf6c3624h, 0bf6c5cd4h, 0bf6c835eh, 0bf6ca9c5h
dd	0bf6cd007h, 0bf6cf624h, 0bf6d1c1dh, 0bf6d41f2h, 0bf6d67a1h, 0bf6d8d2dh
dd	0bf6db293h, 0bf6dd7d5h, 0bf6dfcf2h, 0bf6e21ebh, 0bf6e46beh, 0bf6e6b6dh
dd	0bf6e8ff8h, 0bf6eb45dh, 0bf6ed89eh, 0bf6efcbah, 0bf6f20b0h, 0bf6f4483h
dd	0bf6f6830h, 0bf6f8bb8h, 0bf6faf1bh, 0bf6fd25ah, 0bf6ff573h, 0bf701867h
dd	0bf703b37h, 0bf705de1h, 0bf708066h, 0bf70a2c6h, 0bf70c501h, 0bf70e717h
dd	0bf710908h, 0bf712ad4h, 0bf714c7ah, 0bf716dfbh, 0bf718f57h, 0bf71b08eh
dd	0bf71d19fh, 0bf71f28ch, 0bf721352h, 0bf7233f4h, 0bf725470h, 0bf7274c7h
dd	0bf7294f8h, 0bf72b504h, 0bf72d4ebh, 0bf72f4ach, 0bf731447h, 0bf7333beh
dd	0bf73530eh, 0bf737239h, 0bf73913fh, 0bf73b01fh, 0bf73ced9h, 0bf73ed6eh
dd	0bf740bddh, 0bf742a27h, 0bf74484bh, 0bf746649h, 0bf748422h, 0bf74a1d5h
dd	0bf74bf62h, 0bf74dcc9h, 0bf74fa0bh, 0bf751727h, 0bf75341dh, 0bf7550edh
dd	0bf756d97h, 0bf758a1ch, 0bf75a67bh, 0bf75c2b3h, 0bf75dec6h, 0bf75fab3h
dd	0bf76167ah, 0bf76321bh, 0bf764d97h, 0bf7668ech, 0bf76841bh, 0bf769f24h
dd	0bf76ba07h, 0bf76d4c4h, 0bf76ef5bh, 0bf7709cch, 0bf772417h, 0bf773e3ch
dd	0bf77583ah, 0bf777213h, 0bf778bc5h, 0bf77a551h, 0bf77beb7h, 0bf77d7f7h
dd	0bf77f110h, 0bf780a04h, 0bf7822d1h, 0bf783b77h, 0bf7853f8h, 0bf786c52h
dd	0bf788486h, 0bf789c93h, 0bf78b47bh, 0bf78cc3bh, 0bf78e3d6h, 0bf78fb4ah
dd	0bf791298h, 0bf7929bfh, 0bf7940c0h, 0bf79579ah, 0bf796e4eh, 0bf7984dch
dd	0bf799b43h, 0bf79b183h, 0bf79c79dh, 0bf79dd91h, 0bf79f35eh, 0bf7a0904h
dd	0bf7a1e84h, 0bf7a33ddh, 0bf7a4910h, 0bf7a5e1ch, 0bf7a7302h, 0bf7a87c1h
dd	0bf7a9c59h, 0bf7ab0cbh, 0bf7ac516h, 0bf7ad93ah, 0bf7aed37h, 0bf7b010eh
dd	0bf7b14beh, 0bf7b2848h, 0bf7b3babh, 0bf7b4ee7h, 0bf7b61fch, 0bf7b74eah
dd	0bf7b87b2h, 0bf7b9a53h, 0bf7baccdh, 0bf7bbf20h, 0bf7bd14dh, 0bf7be353h
dd	0bf7bf531h, 0bf7c06e9h, 0bf7c187ah, 0bf7c29e5h, 0bf7c3b28h, 0bf7c4c44h
dd	0bf7c5d3ah, 0bf7c6e08h, 0bf7c7eb0h, 0bf7c8f31h, 0bf7c9f8ah, 0bf7cafbdh
dd	0bf7cbfc9h, 0bf7ccfaeh, 0bf7cdf6ch, 0bf7cef03h, 0bf7cfe73h, 0bf7d0dbch
dd	0bf7d1cddh, 0bf7d2bd8h, 0bf7d3aach, 0bf7d4959h, 0bf7d57deh, 0bf7d663dh
dd	0bf7d7474h, 0bf7d8285h, 0bf7d906eh, 0bf7d9e30h, 0bf7dabcch, 0bf7db940h
dd	0bf7dc68ch, 0bf7dd3b2h, 0bf7de0b1h, 0bf7ded88h, 0bf7dfa38h, 0bf7e06c2h
dd	0bf7e1324h, 0bf7e1f5eh, 0bf7e2b72h, 0bf7e375eh, 0bf7e4323h, 0bf7e4ec1h
dd	0bf7e5a38h, 0bf7e6588h, 0bf7e70b0h, 0bf7e7bb1h, 0bf7e868bh, 0bf7e913dh
dd	0bf7e9bc9h, 0bf7ea62dh, 0bf7eb069h, 0bf7eba7fh, 0bf7ec46dh, 0bf7ece34h
dd	0bf7ed7d4h, 0bf7ee14ch, 0bf7eea9dh, 0bf7ef3c7h, 0bf7efcc9h, 0bf7f05a4h
dd	0bf7f0e58h, 0bf7f16e4h, 0bf7f1f49h, 0bf7f2787h, 0bf7f2f9dh, 0bf7f378ch
dd	0bf7f3f54h, 0bf7f46f4h, 0bf7f4e6dh, 0bf7f55bfh, 0bf7f5ce9h, 0bf7f63ech
dd	0bf7f6ac7h, 0bf7f717bh, 0bf7f7808h, 0bf7f7e6dh, 0bf7f84abh, 0bf7f8ac2h
dd	0bf7f90b1h, 0bf7f9678h, 0bf7f9c18h, 0bf7fa191h, 0bf7fa6e3h, 0bf7fac0dh
dd	0bf7fb10fh, 0bf7fb5eah, 0bf7fba9eh, 0bf7fbf2ah, 0bf7fc38fh, 0bf7fc7cch
dd	0bf7fcbe2h, 0bf7fcfd1h, 0bf7fd397h, 0bf7fd737h, 0bf7fdaafh, 0bf7fde00h
dd	0bf7fe129h, 0bf7fe42bh, 0bf7fe705h, 0bf7fe9b8h, 0bf7fec43h, 0bf7feea7h
dd	0bf7ff0e3h, 0bf7ff2f8h, 0bf7ff4e6h, 0bf7ff6ach, 0bf7ff84ah, 0bf7ff9c1h
dd	0bf7ffb11h, 0bf7ffc39h, 0bf7ffd39h, 0bf7ffe13h, 0bf7ffec4h, 0bf7fff4eh
dd	0bf7fffb1h, 0bf7fffech, 0bf800000h, 0bf7fffech, 0bf7fffb1h, 0bf7fff4eh
dd	0bf7ffec4h, 0bf7ffe13h, 0bf7ffd39h, 0bf7ffc39h, 0bf7ffb11h, 0bf7ff9c1h
dd	0bf7ff84ah, 0bf7ff6ach, 0bf7ff4e6h, 0bf7ff2f8h, 0bf7ff0e3h, 0bf7feea7h
dd	0bf7fec43h, 0bf7fe9b8h, 0bf7fe705h, 0bf7fe42bh, 0bf7fe129h, 0bf7fde00h
dd	0bf7fdaafh, 0bf7fd737h, 0bf7fd397h, 0bf7fcfd1h, 0bf7fcbe2h, 0bf7fc7cch
dd	0bf7fc38fh, 0bf7fbf2ah, 0bf7fba9eh, 0bf7fb5eah, 0bf7fb10fh, 0bf7fac0dh
dd	0bf7fa6e3h, 0bf7fa191h, 0bf7f9c18h, 0bf7f9678h, 0bf7f90b1h, 0bf7f8ac2h
dd	0bf7f84abh, 0bf7f7e6dh, 0bf7f7808h, 0bf7f717bh, 0bf7f6ac7h, 0bf7f63ech
dd	0bf7f5ce9h, 0bf7f55bfh, 0bf7f4e6dh, 0bf7f46f4h, 0bf7f3f54h, 0bf7f378ch
dd	0bf7f2f9dh, 0bf7f2787h, 0bf7f1f49h, 0bf7f16e4h, 0bf7f0e58h, 0bf7f05a4h
dd	0bf7efcc9h, 0bf7ef3c7h, 0bf7eea9dh, 0bf7ee14ch, 0bf7ed7d4h, 0bf7ece34h
dd	0bf7ec46dh, 0bf7eba7fh, 0bf7eb069h, 0bf7ea62dh, 0bf7e9bc9h, 0bf7e913dh
dd	0bf7e868bh, 0bf7e7bb1h, 0bf7e70b0h, 0bf7e6588h, 0bf7e5a38h, 0bf7e4ec1h
dd	0bf7e4323h, 0bf7e375eh, 0bf7e2b72h, 0bf7e1f5eh, 0bf7e1324h, 0bf7e06c2h
dd	0bf7dfa38h, 0bf7ded88h, 0bf7de0b1h, 0bf7dd3b2h, 0bf7dc68ch, 0bf7db940h
dd	0bf7dabcch, 0bf7d9e30h, 0bf7d906eh, 0bf7d8285h, 0bf7d7474h, 0bf7d663dh
dd	0bf7d57deh, 0bf7d4959h, 0bf7d3aach, 0bf7d2bd8h, 0bf7d1cddh, 0bf7d0dbch
dd	0bf7cfe73h, 0bf7cef03h, 0bf7cdf6ch, 0bf7ccfaeh, 0bf7cbfc9h, 0bf7cafbdh
dd	0bf7c9f8ah, 0bf7c8f31h, 0bf7c7eb0h, 0bf7c6e08h, 0bf7c5d3ah, 0bf7c4c44h
dd	0bf7c3b28h, 0bf7c29e5h, 0bf7c187ah, 0bf7c06e9h, 0bf7bf531h, 0bf7be353h
dd	0bf7bd14dh, 0bf7bbf20h, 0bf7baccdh, 0bf7b9a53h, 0bf7b87b2h, 0bf7b74eah
dd	0bf7b61fch, 0bf7b4ee7h, 0bf7b3babh, 0bf7b2848h, 0bf7b14beh, 0bf7b010eh
dd	0bf7aed37h, 0bf7ad93ah, 0bf7ac516h, 0bf7ab0cbh, 0bf7a9c59h, 0bf7a87c1h
dd	0bf7a7302h, 0bf7a5e1ch, 0bf7a4910h, 0bf7a33ddh, 0bf7a1e84h, 0bf7a0904h
dd	0bf79f35eh, 0bf79dd91h, 0bf79c79dh, 0bf79b183h, 0bf799b43h, 0bf7984dch
dd	0bf796e4eh, 0bf79579ah, 0bf7940c0h, 0bf7929bfh, 0bf791298h, 0bf78fb4ah
dd	0bf78e3d6h, 0bf78cc3bh, 0bf78b47bh, 0bf789c93h, 0bf788486h, 0bf786c52h
dd	0bf7853f8h, 0bf783b77h, 0bf7822d1h, 0bf780a04h, 0bf77f110h, 0bf77d7f7h
dd	0bf77beb7h, 0bf77a551h, 0bf778bc5h, 0bf777213h, 0bf77583ah, 0bf773e3ch
dd	0bf772417h, 0bf7709cch, 0bf76ef5bh, 0bf76d4c4h, 0bf76ba07h, 0bf769f24h
dd	0bf76841bh, 0bf7668ech, 0bf764d97h, 0bf76321bh, 0bf76167ah, 0bf75fab3h
dd	0bf75dec6h, 0bf75c2b3h, 0bf75a67bh, 0bf758a1ch, 0bf756d97h, 0bf7550edh
dd	0bf75341dh, 0bf751727h, 0bf74fa0bh, 0bf74dcc9h, 0bf74bf62h, 0bf74a1d5h
dd	0bf748422h, 0bf746649h, 0bf74484bh, 0bf742a27h, 0bf740bddh, 0bf73ed6eh
dd	0bf73ced9h, 0bf73b01fh, 0bf73913fh, 0bf737239h, 0bf73530eh, 0bf7333beh
dd	0bf731447h, 0bf72f4ach, 0bf72d4ebh, 0bf72b504h, 0bf7294f8h, 0bf7274c7h
dd	0bf725470h, 0bf7233f4h, 0bf721352h, 0bf71f28ch, 0bf71d19fh, 0bf71b08eh
dd	0bf718f57h, 0bf716dfbh, 0bf714c7ah, 0bf712ad4h, 0bf710908h, 0bf70e717h
dd	0bf70c501h, 0bf70a2c6h, 0bf708066h, 0bf705de1h, 0bf703b37h, 0bf701867h
dd	0bf6ff573h, 0bf6fd25ah, 0bf6faf1bh, 0bf6f8bb8h, 0bf6f6830h, 0bf6f4483h
dd	0bf6f20b0h, 0bf6efcbah, 0bf6ed89eh, 0bf6eb45dh, 0bf6e8ff8h, 0bf6e6b6dh
dd	0bf6e46beh, 0bf6e21ebh, 0bf6dfcf2h, 0bf6dd7d5h, 0bf6db293h, 0bf6d8d2dh
dd	0bf6d67a1h, 0bf6d41f2h, 0bf6d1c1dh, 0bf6cf624h, 0bf6cd007h, 0bf6ca9c5h
dd	0bf6c835eh, 0bf6c5cd4h, 0bf6c3624h, 0bf6c0f50h, 0bf6be858h, 0bf6bc13bh
dd	0bf6b99fbh, 0bf6b7295h, 0bf6b4b0ch, 0bf6b235eh, 0bf6afb8ch, 0bf6ad395h
dd	0bf6aab7bh, 0bf6a833ch, 0bf6a5ad9h, 0bf6a3252h, 0bf6a09a7h, 0bf69e0d7h
dd	0bf69b7e4h, 0bf698ecch, 0bf696591h, 0bf693c32h, 0bf6912aeh, 0bf68e907h
dd	0bf68bf3ch, 0bf68954ch, 0bf686b39h, 0bf684103h, 0bf6816a8h, 0bf67ec29h
dd	0bf67c187h, 0bf6796c1h, 0bf676bd8h, 0bf6740cah, 0bf671599h, 0bf66ea45h
dd	0bf66becch, 0bf669330h, 0bf666771h, 0bf663b8eh, 0bf660f88h, 0bf65e35eh
dd	0bf65b710h, 0bf658aa0h, 0bf655e0bh, 0bf653154h, 0bf650479h, 0bf64d77bh
dd	0bf64aa59h, 0bf647d14h, 0bf644fach, 0bf642221h, 0bf63f473h, 0bf63c6a1h
dd	0bf6398ach, 0bf636a95h, 0bf633c5ah, 0bf630dfch, 0bf62df7bh, 0bf62b0d7h
dd	0bf628210h, 0bf625326h, 0bf62241ah, 0bf61f4eah, 0bf61c598h, 0bf619622h
dd	0bf61668ah, 0bf6136d0h, 0bf6106f2h, 0bf60d6f2h, 0bf60a6cfh, 0bf607689h
dd	0bf604621h, 0bf601596h, 0bf5fe4e9h, 0bf5fb419h, 0bf5f8327h, 0bf5f5212h
dd	0bf5f20dbh, 0bf5eef81h, 0bf5ebe05h, 0bf5e8c67h, 0bf5e5aa6h, 0bf5e28c3h
dd	0bf5df6beh, 0bf5dc497h, 0bf5d924dh, 0bf5d5fe1h, 0bf5d2d53h, 0bf5cfaa3h
dd	0bf5cc7d1h, 0bf5c94ddh, 0bf5c61c7h, 0bf5c2e8eh, 0bf5bfb34h, 0bf5bc7b8h
dd	0bf5b941ah, 0bf5b605ah, 0bf5b2c79h, 0bf5af875h, 0bf5ac450h, 0bf5a9009h
dd	0bf5a5ba0h, 0bf5a2716h, 0bf59f26ah, 0bf59bd9ch, 0bf5988adh, 0bf59539ch
dd	0bf591e6ah, 0bf58e916h, 0bf58b3a1h, 0bf587e0bh, 0bf584853h, 0bf581279h
dd	0bf57dc7fh, 0bf57a663h, 0bf577026h, 0bf5739c7h, 0bf570348h, 0bf56cca7h
dd	0bf5695e5h, 0bf565f02h, 0bf5627feh, 0bf55f0d9h, 0bf55b993h, 0bf55822ch
dd	0bf554aa4h, 0bf5512fbh, 0bf54db31h, 0bf54a347h, 0bf546b3bh, 0bf54330fh
dd	0bf53fac3h, 0bf53c255h, 0bf5389c7h, 0bf535118h, 0bf531849h, 0bf52df59h
dd	0bf52a649h, 0bf526d18h, 0bf5233c6h, 0bf51fa54h, 0bf51c0c2h, 0bf518710h
dd	0bf514d3dh, 0bf51134ah, 0bf50d937h, 0bf509f03h, 0bf5064afh, 0bf502a3bh
dd	0bf4fefa8h, 0bf4fb4f4h, 0bf4f7a1fh, 0bf4f3f2bh, 0bf4f0417h, 0bf4ec8e4h
dd	0bf4e8d90h, 0bf4e521ch, 0bf4e1689h, 0bf4ddad5h, 0bf4d9f02h, 0bf4d6310h
dd	0bf4d26fdh, 0bf4ceacbh, 0bf4cae79h, 0bf4c7208h, 0bf4c3578h, 0bf4bf8c7h
dd	0bf4bbbf8h, 0bf4b7f09h, 0bf4b41fah, 0bf4b04cch, 0bf4ac77fh, 0bf4a8a13h
dd	0bf4a4c87h, 0bf4a0edch, 0bf49d112h, 0bf499329h, 0bf495521h, 0bf4916fah
dd	0bf48d8b3h, 0bf489a4eh, 0bf485bcah, 0bf481d27h, 0bf47de65h, 0bf479f84h
dd	0bf476085h, 0bf472167h, 0bf46e22ah, 0bf46a2ceh, 0bf466354h, 0bf4623bbh
dd	0bf45e403h, 0bf45a42dh, 0bf456439h, 0bf452426h, 0bf44e3f5h, 0bf44a3a5h
dd	0bf446337h, 0bf4422abh, 0bf43e200h, 0bf43a138h, 0bf436051h, 0bf431f4ch
dd	0bf42de29h, 0bf429ce8h, 0bf425b89h, 0bf421a0bh, 0bf41d870h, 0bf4196b7h
dd	0bf4154e1h, 0bf4112ech, 0bf40d0dah, 0bf408ea9h, 0bf404c5ch, 0bf4009f0h
dd	0bf3fc767h, 0bf3f84c0h, 0bf3f41fch, 0bf3eff1bh, 0bf3ebc1bh, 0bf3e78ffh
dd	0bf3e35c5h, 0bf3df26eh, 0bf3daef9h, 0bf3d6b67h, 0bf3d27b8h, 0bf3ce3ech
dd	0bf3ca003h, 0bf3c5bfch, 0bf3c17d9h, 0bf3bd398h, 0bf3b8f3bh, 0bf3b4ac1h
dd	0bf3b0629h, 0bf3ac175h, 0bf3a7ca4h, 0bf3a37b7h, 0bf39f2ach, 0bf39ad85h
dd	0bf396842h, 0bf3922e1h, 0bf38dd65h, 0bf3897cbh, 0bf385216h, 0bf380c43h
dd	0bf37c655h, 0bf37804ah, 0bf373a23h, 0bf36f3dfh, 0bf36ad7fh, 0bf366704h
dd	0bf36206ch, 0bf35d9b8h, 0bf3592e7h, 0bf354bfbh, 0bf3504f3h, 0bf34bdcfh
dd	0bf34768fh, 0bf342f34h, 0bf33e7bch, 0bf33a029h, 0bf33587ah, 0bf3310afh
dd	0bf32c8c9h, 0bf3280c7h, 0bf3238aah, 0bf31f071h, 0bf31a81dh, 0bf315fadh
dd	0bf311722h, 0bf30ce7ch, 0bf3085bbh, 0bf303cdeh, 0bf2ff3e6h, 0bf2faad3h
dd	0bf2f61a5h, 0bf2f185bh, 0bf2ecef7h, 0bf2e8578h, 0bf2e3bdeh, 0bf2df229h
dd	0bf2da859h, 0bf2d5e6fh, 0bf2d1469h, 0bf2cca49h, 0bf2c800fh, 0bf2c35b9h
dd	0bf2beb4ah, 0bf2ba0bfh, 0bf2b561bh, 0bf2b0b5bh, 0bf2ac082h, 0bf2a758eh
dd	0bf2a2a80h, 0bf29df57h, 0bf299415h, 0bf2948b8h, 0bf28fd41h, 0bf28b1b0h
dd	0bf286605h, 0bf281a40h, 0bf27ce61h, 0bf278268h, 0bf273656h, 0bf26ea2ah
dd	0bf269de3h, 0bf265184h, 0bf26050ah, 0bf25b877h, 0bf256bcbh, 0bf251f04h
dd	0bf24d225h, 0bf24852ch, 0bf24381ah, 0bf23eaeeh, 0bf239da9h, 0bf23504bh
dd	0bf2302d3h, 0bf22b543h, 0bf226799h, 0bf2219d7h, 0bf21cbfbh, 0bf217e06h
dd	0bf212ff9h, 0bf20e1d2h, 0bf209393h, 0bf20453bh, 0bf1ff6cbh, 0bf1fa841h
dd	0bf1f599fh, 0bf1f0ae5h, 0bf1ebc12h, 0bf1e6d26h, 0bf1e1e22h, 0bf1dcf06h
dd	0bf1d7fd1h, 0bf1d3084h, 0bf1ce11fh, 0bf1c91a2h, 0bf1c420ch, 0bf1bf25fh
dd	0bf1ba299h, 0bf1b52bbh, 0bf1b02c6h, 0bf1ab2b8h, 0bf1a6293h, 0bf1a1255h
dd	0bf19c200h, 0bf197194h, 0bf19210fh, 0bf18d073h, 0bf187fc0h, 0bf182ef5h
dd	0bf17de12h, 0bf178d18h, 0bf173c07h, 0bf16eadeh, 0bf16999fh, 0bf164847h
dd	0bf15f6d9h, 0bf15a554h, 0bf1553b7h, 0bf150204h, 0bf14b039h, 0bf145e58h
dd	0bf140c5fh, 0bf13ba50h, 0bf13682ah, 0bf1315eeh, 0bf12c39ah, 0bf127130h
dd	0bf121eb0h, 0bf11cc19h, 0bf11796bh, 0bf1126a7h, 0bf10d3cdh, 0bf1080dch
dd	0bf102dd5h, 0bf0fdab8h, 0bf0f8784h, 0bf0f343bh, 0bf0ee0dbh, 0bf0e8d65h
dd	0bf0e39dah, 0bf0de638h, 0bf0d9281h, 0bf0d3eb3h, 0bf0cead0h, 0bf0c96d7h
dd	0bf0c42c9h, 0bf0beea5h, 0bf0b9a6bh, 0bf0b461ch, 0bf0af1b7h, 0bf0a9d3dh
dd	0bf0a48adh, 0bf09f409h, 0bf099f4eh, 0bf094a7fh, 0bf08f59bh, 0bf08a0a1h
dd	0bf084b92h, 0bf07f66fh, 0bf07a136h, 0bf074be8h, 0bf06f686h, 0bf06a10eh
dd	0bf064b82h, 0bf05f5e2h, 0bf05a02ch, 0bf054a62h, 0bf04f484h, 0bf049e91h
dd	0bf044889h, 0bf03f26dh, 0bf039c3dh, 0bf0345f8h, 0bf02ef9fh, 0bf029932h
dd	0bf0242b1h, 0bf01ec1ch, 0bf019573h, 0bf013eb5h, 0bf00e7e4h, 0bf0090ffh
dd	0bf003a06h, 0beffc5f3h, 0beff17b2h, 0befe694ah, 0befdbabbh, 0befd0c04h
dd	0befc5d27h, 0befbae22h, 0befafef7h, 0befa4fa5h, 0bef9a02dh, 0bef8f08eh
dd	0bef840c8h, 0bef790dch, 0bef6e0cbh, 0bef63093h, 0bef58035h, 0bef4cfb1h
dd	0bef41f07h, 0bef36e38h, 0bef2bd43h, 0bef20c29h, 0bef15aeah, 0bef0a985h
dd	0beeff7fbh, 0beef464ch, 0beee9479h, 0beede280h, 0beed3063h, 0beec7e21h
dd	0beebcbbbh, 0beeb1930h, 0beea6681h, 0bee9b3aeh, 0bee900b7h, 0bee84d9ch
dd	0bee79a5dh, 0bee6e6fbh, 0bee63375h, 0bee57fcbh, 0bee4cbfeh, 0bee4180eh
dd	0bee363fah, 0bee2afc4h, 0bee1fb6ah, 0bee146eeh, 0bee0924fh, 0bedfdd8dh
dd	0bedf28a9h, 0bede73a2h, 0beddbe79h, 0bedd092eh, 0bedc53c1h, 0bedb9e31h
dd	0bedae880h, 0beda32adh, 0bed97cb9h, 0bed8c6a3h, 0bed8106bh, 0bed75a13h
dd	0bed6a399h, 0bed5ecfdh, 0bed53641h, 0bed47f64h, 0bed3c867h, 0bed31148h
dd	0bed25a09h, 0bed1a2aah, 0bed0eb2ah, 0bed0338ah, 0becf7bcah, 0becec3eah
dd	0bece0beah, 0becd53cah, 0becc9b8bh, 0becbe32ch, 0becb2aaeh, 0beca7210h
dd	0bec9b953h, 0bec90077h, 0bec8477ch, 0bec78e62h, 0bec6d529h, 0bec61bd2h
dd	0bec5625ch, 0bec4a8c8h, 0bec3ef15h, 0bec33544h, 0bec27b55h, 0bec1c148h
dd	0bec1071eh, 0bec04cd5h, 0bebf926fh, 0bebed7ebh, 0bebe1d4ah, 0bebd628bh
dd	0bebca7afh, 0bebbecb6h, 0bebb31a0h, 0beba766eh, 0beb9bb1eh, 0beb8ffb2h
dd	0beb8442ah, 0beb78884h, 0beb6ccc3h, 0beb610e6h, 0beb554ech, 0beb498d6h
dd	0beb3dca5h, 0beb32058h, 0beb263efh, 0beb1a76bh, 0beb0eacbh, 0beb02e10h
dd	0beaf713ah, 0beaeb449h, 0beadf73ch, 0bead3a15h, 0beac7cd4h, 0beabbf77h
dd	0beab0201h, 0beaa446fh, 0bea986c4h, 0bea8c8feh, 0bea80b1fh, 0bea74d25h
dd	0bea68f12h, 0bea5d0e5h, 0bea5129fh, 0bea4543fh, 0bea395c5h, 0bea2d733h
dd	0bea21887h, 0bea159c2h, 0bea09ae5h, 0be9fdbeeh, 0be9f1cdfh, 0be9e5db8h
dd	0be9d9e78h, 0be9cdf20h, 0be9c1fafh, 0be9b6027h, 0be9aa086h, 0be99e0ceh
dd	0be9920feh, 0be986116h, 0be97a117h, 0be96e100h, 0be9620d2h, 0be95608dh
dd	0be94a031h, 0be93dfbfh, 0be931f35h, 0be925e94h, 0be919dddh, 0be90dd10h
dd	0be901c2ch, 0be8f5b32h, 0be8e9a22h, 0be8dd8fch, 0be8d17c0h, 0be8c566eh
dd	0be8b9507h, 0be8ad38ah, 0be8a11f7h, 0be895050h, 0be888e93h, 0be87ccc1h
dd	0be870adah, 0be8648dfh, 0be8586ceh, 0be84c4aah, 0be840270h, 0be834022h
dd	0be827dc0h, 0be81bb4ah, 0be80f8c0h, 0be803622h, 0be7ee6e1h, 0be7d6156h
dd	0be7bdba4h, 0be7a55cbh, 0be78cfcch, 0be7749a6h, 0be75c35ah, 0be743ce8h
dd	0be72b651h, 0be712f94h, 0be6fa8b2h, 0be6e21abh, 0be6c9a7fh, 0be6b132fh
dd	0be698bbah, 0be680422h, 0be667c66h, 0be64f486h, 0be636c83h, 0be61e45ch
dd	0be605c13h, 0be5ed3a8h, 0be5d4b1ah, 0be5bc26ah, 0be5a3997h, 0be58b0a4h
dd	0be57278fh, 0be559e58h, 0be541501h, 0be528b89h, 0be5101f1h, 0be4f7838h
dd	0be4dee60h, 0be4c6467h, 0be4ada4fh, 0be495018h, 0be47c5c2h, 0be463b4dh
dd	0be44b0b9h, 0be432607h, 0be419b37h, 0be401049h, 0be3e853eh, 0be3cfa15h
dd	0be3b6ecfh, 0be39e36ch, 0be3857ech, 0be36cc50h, 0be354098h, 0be33b4c4h
dd	0be3228d4h, 0be309cc9h, 0be2f10a2h, 0be2d8461h, 0be2bf804h, 0be2a6b8dh
dd	0be28defch, 0be275251h, 0be25c58ch, 0be2438adh, 0be22abb6h, 0be211ea5h
dd	0be1f917bh, 0be1e0438h, 0be1c76deh, 0be1ae96bh, 0be195be0h, 0be17ce3dh
dd	0be164083h, 0be14b2b2h, 0be1324cah, 0be1196cch, 0be1008b7h, 0be0e7a8bh
dd	0be0cec4ah, 0be0b5df3h, 0be09cf86h, 0be084105h, 0be06b26eh, 0be0523c2h
dd	0be039502h, 0be02062eh, 0be007745h, 0bdfdd092h, 0bdfab273h, 0bdf7942ch
dd	0bdf475c0h, 0bdf1572eh, 0bdee3876h, 0bdeb199ah, 0bde7fa9ah, 0bde4db76h
dd	0bde1bc2eh, 0bdde9cc4h, 0bddb7d37h, 0bdd85d89h, 0bdd53db9h, 0bdd21dc8h
dd	0bdcefdb7h, 0bdcbdd86h, 0bdc8bd36h, 0bdc59cc6h, 0bdc27c39h, 0bdbf5b8dh
dd	0bdbc3ac3h, 0bdb919ddh, 0bdb5f8dah, 0bdb2d7bbh, 0bdafb680h, 0bdac952bh
dd	0bda973bah, 0bda65230h, 0bda3308ch, 0bda00ecfh, 0bd9cecf9h, 0bd99cb0ah
dd	0bd96a905h, 0bd9386e7h, 0bd9064b4h, 0bd8d426ah, 0bd8a200ah, 0bd86fd94h
dd	0bd83db0ah, 0bd80b86ch, 0bd7b2b74h, 0bd74e5e9h, 0bd6ea038h, 0bd685a62h
dd	0bd621469h, 0bd5bce4ch, 0bd55880eh, 0bd4f41afh, 0bd48fb30h, 0bd42b491h
dd	0bd3c6dd5h, 0bd3626fch, 0bd2fe007h, 0bd2998f6h, 0bd2351cbh, 0bd1d0a88h
dd	0bd16c32ch, 0bd107bb8h, 0bd0a342fh, 0bd03ec90h, 0bcfb49bah, 0bceeba2ch
dd	0bce22a7ah, 0bcd59aa6h, 0bcc90ab0h, 0bcbc7a9bh, 0bcafea69h, 0bca35a1ch
dd	0bc96c9b6h, 0bc8a3938h, 0bc7b514bh, 0bc622fffh, 0bc490e90h, 0bc2fed02h
dd	0bc16cb58h, 0bbfb5330h, 0bbc90f88h, 0bb96cbc1h, 0bb490fc6h, 0bac90fd5h
dd	000000000h, 03ac90fd5h, 03b490fc6h, 03b96cbc1h, 03bc90f88h, 03bfb5330h
dd	03c16cb58h, 03c2fed02h, 03c490e90h, 03c622fffh, 03c7b514bh, 03c8a3938h
dd	03c96c9b6h, 03ca35a1ch, 03cafea69h, 03cbc7a9bh, 03cc90ab0h, 03cd59aa6h
dd	03ce22a7ah, 03ceeba2ch, 03cfb49bah, 03d03ec90h, 03d0a342fh, 03d107bb8h
dd	03d16c32ch, 03d1d0a88h, 03d2351cbh, 03d2998f6h, 03d2fe007h, 03d3626fch
dd	03d3c6dd5h, 03d42b491h, 03d48fb30h, 03d4f41afh, 03d55880eh, 03d5bce4ch
dd	03d621469h, 03d685a62h, 03d6ea038h, 03d74e5e9h, 03d7b2b74h, 03d80b86ch
dd	03d83db0ah, 03d86fd94h, 03d8a200ah, 03d8d426ah, 03d9064b4h, 03d9386e7h
dd	03d96a905h, 03d99cb0ah, 03d9cecf9h, 03da00ecfh, 03da3308ch, 03da65230h
dd	03da973bah, 03dac952bh, 03dafb680h, 03db2d7bbh, 03db5f8dah, 03db919ddh
dd	03dbc3ac3h, 03dbf5b8dh, 03dc27c39h, 03dc59cc6h, 03dc8bd36h, 03dcbdd86h
dd	03dcefdb7h, 03dd21dc8h, 03dd53db9h, 03dd85d89h, 03ddb7d37h, 03dde9cc4h
dd	03de1bc2eh, 03de4db76h, 03de7fa9ah, 03deb199ah, 03dee3876h, 03df1572eh
dd	03df475c0h, 03df7942ch, 03dfab273h, 03dfdd092h, 03e007745h, 03e02062eh
dd	03e039502h, 03e0523c2h, 03e06b26eh, 03e084105h, 03e09cf86h, 03e0b5df3h
dd	03e0cec4ah, 03e0e7a8bh, 03e1008b7h, 03e1196cch, 03e1324cah, 03e14b2b2h
dd	03e164083h, 03e17ce3dh, 03e195be0h, 03e1ae96bh, 03e1c76deh, 03e1e0438h
dd	03e1f917bh, 03e211ea5h, 03e22abb6h, 03e2438adh, 03e25c58ch, 03e275251h
dd	03e28defch, 03e2a6b8dh, 03e2bf804h, 03e2d8461h, 03e2f10a2h, 03e309cc9h
dd	03e3228d4h, 03e33b4c4h, 03e354098h, 03e36cc50h, 03e3857ech, 03e39e36ch
dd	03e3b6ecfh, 03e3cfa15h, 03e3e853eh, 03e401049h, 03e419b37h, 03e432607h
dd	03e44b0b9h, 03e463b4dh, 03e47c5c2h, 03e495018h, 03e4ada4fh, 03e4c6467h
dd	03e4dee60h, 03e4f7838h, 03e5101f1h, 03e528b89h, 03e541501h, 03e559e58h
dd	03e57278fh, 03e58b0a4h, 03e5a3997h, 03e5bc26ah, 03e5d4b1ah, 03e5ed3a8h
dd	03e605c13h, 03e61e45ch, 03e636c83h, 03e64f486h, 03e667c66h, 03e680422h
dd	03e698bbah, 03e6b132fh, 03e6c9a7fh, 03e6e21abh, 03e6fa8b2h, 03e712f94h
dd	03e72b651h, 03e743ce8h, 03e75c35ah, 03e7749a6h, 03e78cfcch, 03e7a55cbh
dd	03e7bdba4h, 03e7d6156h, 03e7ee6e1h, 03e803622h, 03e80f8c0h, 03e81bb4ah
dd	03e827dc0h, 03e834022h, 03e840270h, 03e84c4aah, 03e8586ceh, 03e8648dfh
dd	03e870adah, 03e87ccc1h, 03e888e93h, 03e895050h, 03e8a11f7h, 03e8ad38ah
dd	03e8b9507h, 03e8c566eh, 03e8d17c0h, 03e8dd8fch, 03e8e9a22h, 03e8f5b32h
dd	03e901c2ch, 03e90dd10h, 03e919dddh, 03e925e94h, 03e931f35h, 03e93dfbfh
dd	03e94a031h, 03e95608dh, 03e9620d2h, 03e96e100h, 03e97a117h, 03e986116h
dd	03e9920feh, 03e99e0ceh, 03e9aa086h, 03e9b6027h, 03e9c1fafh, 03e9cdf20h
dd	03e9d9e78h, 03e9e5db8h, 03e9f1cdfh, 03e9fdbeeh, 03ea09ae5h, 03ea159c2h
dd	03ea21887h, 03ea2d733h, 03ea395c5h, 03ea4543fh, 03ea5129fh, 03ea5d0e5h
dd	03ea68f12h, 03ea74d25h, 03ea80b1fh, 03ea8c8feh, 03ea986c4h, 03eaa446fh
dd	03eab0201h, 03eabbf77h, 03eac7cd4h, 03ead3a15h, 03eadf73ch, 03eaeb449h
dd	03eaf713ah, 03eb02e10h, 03eb0eacbh, 03eb1a76bh, 03eb263efh, 03eb32058h
dd	03eb3dca5h, 03eb498d6h, 03eb554ech, 03eb610e6h, 03eb6ccc3h, 03eb78884h
dd	03eb8442ah, 03eb8ffb2h, 03eb9bb1eh, 03eba766eh, 03ebb31a0h, 03ebbecb6h
dd	03ebca7afh, 03ebd628bh, 03ebe1d4ah, 03ebed7ebh, 03ebf926fh, 03ec04cd5h
dd	03ec1071eh, 03ec1c148h, 03ec27b55h, 03ec33544h, 03ec3ef15h, 03ec4a8c8h
dd	03ec5625ch, 03ec61bd2h, 03ec6d529h, 03ec78e62h, 03ec8477ch, 03ec90077h
dd	03ec9b953h, 03eca7210h, 03ecb2aaeh, 03ecbe32ch, 03ecc9b8bh, 03ecd53cah
dd	03ece0beah, 03ecec3eah, 03ecf7bcah, 03ed0338ah, 03ed0eb2ah, 03ed1a2aah
dd	03ed25a09h, 03ed31148h, 03ed3c867h, 03ed47f64h, 03ed53641h, 03ed5ecfdh
dd	03ed6a399h, 03ed75a13h, 03ed8106bh, 03ed8c6a3h, 03ed97cb9h, 03eda32adh
dd	03edae880h, 03edb9e31h, 03edc53c1h, 03edd092eh, 03eddbe79h, 03ede73a2h
dd	03edf28a9h, 03edfdd8dh, 03ee0924fh, 03ee146eeh, 03ee1fb6ah, 03ee2afc4h
dd	03ee363fah, 03ee4180eh, 03ee4cbfeh, 03ee57fcbh, 03ee63375h, 03ee6e6fbh
dd	03ee79a5dh, 03ee84d9ch, 03ee900b7h, 03ee9b3aeh, 03eea6681h, 03eeb1930h
dd	03eebcbbbh, 03eec7e21h, 03eed3063h, 03eede280h, 03eee9479h, 03eef464ch
dd	03eeff7fbh, 03ef0a985h, 03ef15aeah, 03ef20c29h, 03ef2bd43h, 03ef36e38h
dd	03ef41f07h, 03ef4cfb1h, 03ef58035h, 03ef63093h, 03ef6e0cbh, 03ef790dch
dd	03ef840c8h, 03ef8f08eh, 03ef9a02dh, 03efa4fa5h, 03efafef7h, 03efbae22h
dd	03efc5d27h, 03efd0c04h, 03efdbabbh, 03efe694ah, 03eff17b2h, 03effc5f3h
dd	03f003a06h, 03f0090ffh, 03f00e7e4h, 03f013eb5h, 03f019573h, 03f01ec1ch
dd	03f0242b1h, 03f029932h, 03f02ef9fh, 03f0345f8h, 03f039c3dh, 03f03f26dh
dd	03f044889h, 03f049e91h, 03f04f484h, 03f054a62h, 03f05a02ch, 03f05f5e2h
dd	03f064b82h, 03f06a10eh, 03f06f686h, 03f074be8h, 03f07a136h, 03f07f66fh
dd	03f084b92h, 03f08a0a1h, 03f08f59bh, 03f094a7fh, 03f099f4eh, 03f09f409h
dd	03f0a48adh, 03f0a9d3dh, 03f0af1b7h, 03f0b461ch, 03f0b9a6bh, 03f0beea5h
dd	03f0c42c9h, 03f0c96d7h, 03f0cead0h, 03f0d3eb3h, 03f0d9281h, 03f0de638h
dd	03f0e39dah, 03f0e8d65h, 03f0ee0dbh, 03f0f343bh, 03f0f8784h, 03f0fdab8h
dd	03f102dd5h, 03f1080dch, 03f10d3cdh, 03f1126a7h, 03f11796bh, 03f11cc19h
dd	03f121eb0h, 03f127130h, 03f12c39ah, 03f1315eeh, 03f13682ah, 03f13ba50h
dd	03f140c5fh, 03f145e58h, 03f14b039h, 03f150204h, 03f1553b7h, 03f15a554h
dd	03f15f6d9h, 03f164847h, 03f16999fh, 03f16eadeh, 03f173c07h, 03f178d18h
dd	03f17de12h, 03f182ef5h, 03f187fc0h, 03f18d073h, 03f19210fh, 03f197194h
dd	03f19c200h, 03f1a1255h, 03f1a6293h, 03f1ab2b8h, 03f1b02c6h, 03f1b52bbh
dd	03f1ba299h, 03f1bf25fh, 03f1c420ch, 03f1c91a2h, 03f1ce11fh, 03f1d3084h
dd	03f1d7fd1h, 03f1dcf06h, 03f1e1e22h, 03f1e6d26h, 03f1ebc12h, 03f1f0ae5h
dd	03f1f599fh, 03f1fa841h, 03f1ff6cbh, 03f20453bh, 03f209393h, 03f20e1d2h
dd	03f212ff9h, 03f217e06h, 03f21cbfbh, 03f2219d7h, 03f226799h, 03f22b543h
dd	03f2302d3h, 03f23504bh, 03f239da9h, 03f23eaeeh, 03f24381ah, 03f24852ch
dd	03f24d225h, 03f251f04h, 03f256bcbh, 03f25b877h, 03f26050ah, 03f265184h
dd	03f269de3h, 03f26ea2ah, 03f273656h, 03f278268h, 03f27ce61h, 03f281a40h
dd	03f286605h, 03f28b1b0h, 03f28fd41h, 03f2948b8h, 03f299415h, 03f29df57h
dd	03f2a2a80h, 03f2a758eh, 03f2ac082h, 03f2b0b5bh, 03f2b561bh, 03f2ba0bfh
dd	03f2beb4ah, 03f2c35b9h, 03f2c800fh, 03f2cca49h, 03f2d1469h, 03f2d5e6fh
dd	03f2da859h, 03f2df229h, 03f2e3bdeh, 03f2e8578h, 03f2ecef7h, 03f2f185bh
dd	03f2f61a5h, 03f2faad3h, 03f2ff3e6h, 03f303cdeh, 03f3085bbh, 03f30ce7ch
dd	03f311722h, 03f315fadh, 03f31a81dh, 03f31f071h, 03f3238aah, 03f3280c7h
dd	03f32c8c9h, 03f3310afh, 03f33587ah, 03f33a029h, 03f33e7bch, 03f342f34h
dd	03f34768fh, 03f34bdcfh, 03f3504f3h, 03f354bfbh, 03f3592e7h, 03f35d9b8h
dd	03f36206ch, 03f366704h, 03f36ad7fh, 03f36f3dfh, 03f373a23h, 03f37804ah
dd	03f37c655h, 03f380c43h, 03f385216h, 03f3897cbh, 03f38dd65h, 03f3922e1h
dd	03f396842h, 03f39ad85h, 03f39f2ach, 03f3a37b7h, 03f3a7ca4h, 03f3ac175h
dd	03f3b0629h, 03f3b4ac1h, 03f3b8f3bh, 03f3bd398h, 03f3c17d9h, 03f3c5bfch
dd	03f3ca003h, 03f3ce3ech, 03f3d27b8h, 03f3d6b67h, 03f3daef9h, 03f3df26eh
dd	03f3e35c5h, 03f3e78ffh, 03f3ebc1bh, 03f3eff1bh, 03f3f41fch, 03f3f84c0h
dd	03f3fc767h, 03f4009f0h, 03f404c5ch, 03f408ea9h, 03f40d0dah, 03f4112ech
dd	03f4154e1h, 03f4196b7h, 03f41d870h, 03f421a0bh, 03f425b89h, 03f429ce8h
dd	03f42de29h, 03f431f4ch, 03f436051h, 03f43a138h, 03f43e200h, 03f4422abh
dd	03f446337h, 03f44a3a5h, 03f44e3f5h, 03f452426h, 03f456439h, 03f45a42dh
dd	03f45e403h, 03f4623bbh, 03f466354h, 03f46a2ceh, 03f46e22ah, 03f472167h
dd	03f476085h, 03f479f84h, 03f47de65h, 03f481d27h, 03f485bcah, 03f489a4eh
dd	03f48d8b3h, 03f4916fah, 03f495521h, 03f499329h, 03f49d112h, 03f4a0edch
dd	03f4a4c87h, 03f4a8a13h, 03f4ac77fh, 03f4b04cch, 03f4b41fah, 03f4b7f09h
dd	03f4bbbf8h, 03f4bf8c7h, 03f4c3578h, 03f4c7208h, 03f4cae79h, 03f4ceacbh
dd	03f4d26fdh, 03f4d6310h, 03f4d9f02h, 03f4ddad5h, 03f4e1689h, 03f4e521ch
dd	03f4e8d90h, 03f4ec8e4h, 03f4f0417h, 03f4f3f2bh, 03f4f7a1fh, 03f4fb4f4h
dd	03f4fefa8h, 03f502a3bh, 03f5064afh, 03f509f03h, 03f50d937h, 03f51134ah
dd	03f514d3dh, 03f518710h, 03f51c0c2h, 03f51fa54h, 03f5233c6h, 03f526d18h
dd	03f52a649h, 03f52df59h, 03f531849h, 03f535118h, 03f5389c7h, 03f53c255h
dd	03f53fac3h, 03f54330fh, 03f546b3bh, 03f54a347h, 03f54db31h, 03f5512fbh
dd	03f554aa4h, 03f55822ch, 03f55b993h, 03f55f0d9h, 03f5627feh, 03f565f02h
dd	03f5695e5h, 03f56cca7h, 03f570348h, 03f5739c7h, 03f577026h, 03f57a663h
dd	03f57dc7fh, 03f581279h, 03f584853h, 03f587e0bh, 03f58b3a1h, 03f58e916h
dd	03f591e6ah, 03f59539ch, 03f5988adh, 03f59bd9ch, 03f59f26ah, 03f5a2716h
dd	03f5a5ba0h, 03f5a9009h, 03f5ac450h, 03f5af875h, 03f5b2c79h, 03f5b605ah
dd	03f5b941ah, 03f5bc7b8h, 03f5bfb34h, 03f5c2e8eh, 03f5c61c7h, 03f5c94ddh
dd	03f5cc7d1h, 03f5cfaa3h, 03f5d2d53h, 03f5d5fe1h, 03f5d924dh, 03f5dc497h
dd	03f5df6beh, 03f5e28c3h, 03f5e5aa6h, 03f5e8c67h, 03f5ebe05h, 03f5eef81h
dd	03f5f20dbh, 03f5f5212h, 03f5f8327h, 03f5fb419h, 03f5fe4e9h, 03f601596h
dd	03f604621h, 03f607689h, 03f60a6cfh, 03f60d6f2h, 03f6106f2h, 03f6136d0h
dd	03f61668ah, 03f619622h, 03f61c598h, 03f61f4eah, 03f62241ah, 03f625326h
dd	03f628210h, 03f62b0d7h, 03f62df7bh, 03f630dfch, 03f633c5ah, 03f636a95h
dd	03f6398ach, 03f63c6a1h, 03f63f473h, 03f642221h, 03f644fach, 03f647d14h
dd	03f64aa59h, 03f64d77bh, 03f650479h, 03f653154h, 03f655e0bh, 03f658aa0h
dd	03f65b710h, 03f65e35eh, 03f660f88h, 03f663b8eh, 03f666771h, 03f669330h
dd	03f66becch, 03f66ea45h, 03f671599h, 03f6740cah, 03f676bd8h, 03f6796c1h
dd	03f67c187h, 03f67ec29h, 03f6816a8h, 03f684103h, 03f686b39h, 03f68954ch
dd	03f68bf3ch, 03f68e907h, 03f6912aeh, 03f693c32h, 03f696591h, 03f698ecch
dd	03f69b7e4h, 03f69e0d7h, 03f6a09a7h, 03f6a3252h, 03f6a5ad9h, 03f6a833ch
dd	03f6aab7bh, 03f6ad395h, 03f6afb8ch, 03f6b235eh, 03f6b4b0ch, 03f6b7295h
dd	03f6b99fbh, 03f6bc13bh, 03f6be858h, 03f6c0f50h, 03f6c3624h, 03f6c5cd4h
dd	03f6c835eh, 03f6ca9c5h, 03f6cd007h, 03f6cf624h, 03f6d1c1dh, 03f6d41f2h
dd	03f6d67a1h, 03f6d8d2dh, 03f6db293h, 03f6dd7d5h, 03f6dfcf2h, 03f6e21ebh
dd	03f6e46beh, 03f6e6b6dh, 03f6e8ff8h, 03f6eb45dh, 03f6ed89eh, 03f6efcbah
dd	03f6f20b0h, 03f6f4483h, 03f6f6830h, 03f6f8bb8h, 03f6faf1bh, 03f6fd25ah
dd	03f6ff573h, 03f701867h, 03f703b37h, 03f705de1h, 03f708066h, 03f70a2c6h
dd	03f70c501h, 03f70e717h, 03f710908h, 03f712ad4h, 03f714c7ah, 03f716dfbh
dd	03f718f57h, 03f71b08eh, 03f71d19fh, 03f71f28ch, 03f721352h, 03f7233f4h
dd	03f725470h, 03f7274c7h, 03f7294f8h, 03f72b504h, 03f72d4ebh, 03f72f4ach
dd	03f731447h, 03f7333beh, 03f73530eh, 03f737239h, 03f73913fh, 03f73b01fh
dd	03f73ced9h, 03f73ed6eh, 03f740bddh, 03f742a27h, 03f74484bh, 03f746649h
dd	03f748422h, 03f74a1d5h, 03f74bf62h, 03f74dcc9h, 03f74fa0bh, 03f751727h
dd	03f75341dh, 03f7550edh, 03f756d97h, 03f758a1ch, 03f75a67bh, 03f75c2b3h
dd	03f75dec6h, 03f75fab3h, 03f76167ah, 03f76321bh, 03f764d97h, 03f7668ech
dd	03f76841bh, 03f769f24h, 03f76ba07h, 03f76d4c4h, 03f76ef5bh, 03f7709cch
dd	03f772417h, 03f773e3ch, 03f77583ah, 03f777213h, 03f778bc5h, 03f77a551h
dd	03f77beb7h, 03f77d7f7h, 03f77f110h, 03f780a04h, 03f7822d1h, 03f783b77h
dd	03f7853f8h, 03f786c52h, 03f788486h, 03f789c93h, 03f78b47bh, 03f78cc3bh
dd	03f78e3d6h, 03f78fb4ah, 03f791298h, 03f7929bfh, 03f7940c0h, 03f79579ah
dd	03f796e4eh, 03f7984dch, 03f799b43h, 03f79b183h, 03f79c79dh, 03f79dd91h
dd	03f79f35eh, 03f7a0904h, 03f7a1e84h, 03f7a33ddh, 03f7a4910h, 03f7a5e1ch
dd	03f7a7302h, 03f7a87c1h, 03f7a9c59h, 03f7ab0cbh, 03f7ac516h, 03f7ad93ah
dd	03f7aed37h, 03f7b010eh, 03f7b14beh, 03f7b2848h, 03f7b3babh, 03f7b4ee7h
dd	03f7b61fch, 03f7b74eah, 03f7b87b2h, 03f7b9a53h, 03f7baccdh, 03f7bbf20h
dd	03f7bd14dh, 03f7be353h, 03f7bf531h, 03f7c06e9h, 03f7c187ah, 03f7c29e5h
dd	03f7c3b28h, 03f7c4c44h, 03f7c5d3ah, 03f7c6e08h, 03f7c7eb0h, 03f7c8f31h
dd	03f7c9f8ah, 03f7cafbdh, 03f7cbfc9h, 03f7ccfaeh, 03f7cdf6ch, 03f7cef03h
dd	03f7cfe73h, 03f7d0dbch, 03f7d1cddh, 03f7d2bd8h, 03f7d3aach, 03f7d4959h
dd	03f7d57deh, 03f7d663dh, 03f7d7474h, 03f7d8285h, 03f7d906eh, 03f7d9e30h
dd	03f7dabcch, 03f7db940h, 03f7dc68ch, 03f7dd3b2h, 03f7de0b1h, 03f7ded88h
dd	03f7dfa38h, 03f7e06c2h, 03f7e1324h, 03f7e1f5eh, 03f7e2b72h, 03f7e375eh
dd	03f7e4323h, 03f7e4ec1h, 03f7e5a38h, 03f7e6588h, 03f7e70b0h, 03f7e7bb1h
dd	03f7e868bh, 03f7e913dh, 03f7e9bc9h, 03f7ea62dh, 03f7eb069h, 03f7eba7fh
dd	03f7ec46dh, 03f7ece34h, 03f7ed7d4h, 03f7ee14ch, 03f7eea9dh, 03f7ef3c7h
dd	03f7efcc9h, 03f7f05a4h, 03f7f0e58h, 03f7f16e4h, 03f7f1f49h, 03f7f2787h
dd	03f7f2f9dh, 03f7f378ch, 03f7f3f54h, 03f7f46f4h, 03f7f4e6dh, 03f7f55bfh
dd	03f7f5ce9h, 03f7f63ech, 03f7f6ac7h, 03f7f717bh, 03f7f7808h, 03f7f7e6dh
dd	03f7f84abh, 03f7f8ac2h, 03f7f90b1h, 03f7f9678h, 03f7f9c18h, 03f7fa191h
dd	03f7fa6e3h, 03f7fac0dh, 03f7fb10fh, 03f7fb5eah, 03f7fba9eh, 03f7fbf2ah
dd	03f7fc38fh, 03f7fc7cch, 03f7fcbe2h, 03f7fcfd1h, 03f7fd397h, 03f7fd737h
dd	03f7fdaafh, 03f7fde00h, 03f7fe129h, 03f7fe42bh, 03f7fe705h, 03f7fe9b8h
dd	03f7fec43h, 03f7feea7h, 03f7ff0e3h, 03f7ff2f8h, 03f7ff4e6h, 03f7ff6ach
dd	03f7ff84ah, 03f7ff9c1h, 03f7ffb11h, 03f7ffc39h, 03f7ffd39h, 03f7ffe13h
dd	03f7ffec4h, 03f7fff4eh, 03f7fffb1h, 03f7fffech, 03f800000h,

;	The
	END

*/

#include <3D/SINTABF.H>
#include <SYSTEM/ADELINE_TYPES.H>

U32 SinTabU32[] = {
	0x00000000, 0x3ac90fd5, 0x3b490fc6, 0x3b96cbc1, 0x3bc90f88, 0x3bfb5330,
	0x3c16cb58, 0x3c2fed02, 0x3c490e90, 0x3c622fff, 0x3c7b514b, 0x3c8a3938,
	0x3c96c9b6, 0x3ca35a1c, 0x3cafea69, 0x3cbc7a9b, 0x3cc90ab0, 0x3cd59aa6,
	0x3ce22a7a, 0x3ceeba2c, 0x3cfb49ba, 0x3d03ec90, 0x3d0a342f, 0x3d107bb8,
	0x3d16c32c, 0x3d1d0a88, 0x3d2351cb, 0x3d2998f6, 0x3d2fe007, 0x3d3626fc,
	0x3d3c6dd5, 0x3d42b491, 0x3d48fb30, 0x3d4f41af, 0x3d55880e, 0x3d5bce4c,
	0x3d621469, 0x3d685a62, 0x3d6ea038, 0x3d74e5e9, 0x3d7b2b74, 0x3d80b86c,
	0x3d83db0a, 0x3d86fd94, 0x3d8a200a, 0x3d8d426a, 0x3d9064b4, 0x3d9386e7,
	0x3d96a905, 0x3d99cb0a, 0x3d9cecf9, 0x3da00ecf, 0x3da3308c, 0x3da65230,
	0x3da973ba, 0x3dac952b, 0x3dafb680, 0x3db2d7bb, 0x3db5f8da, 0x3db919dd,
	0x3dbc3ac3, 0x3dbf5b8d, 0x3dc27c39, 0x3dc59cc6, 0x3dc8bd36, 0x3dcbdd86,
	0x3dcefdb7, 0x3dd21dc8, 0x3dd53db9, 0x3dd85d89, 0x3ddb7d37, 0x3dde9cc4,
	0x3de1bc2e, 0x3de4db76, 0x3de7fa9a, 0x3deb199a, 0x3dee3876, 0x3df1572e,
	0x3df475c0, 0x3df7942c, 0x3dfab273, 0x3dfdd092, 0x3e007745, 0x3e02062e,
	0x3e039502, 0x3e0523c2, 0x3e06b26e, 0x3e084105, 0x3e09cf86, 0x3e0b5df3,
	0x3e0cec4a, 0x3e0e7a8b, 0x3e1008b7, 0x3e1196cc, 0x3e1324ca, 0x3e14b2b2,
	0x3e164083, 0x3e17ce3d, 0x3e195be0, 0x3e1ae96b, 0x3e1c76de, 0x3e1e0438,
	0x3e1f917b, 0x3e211ea5, 0x3e22abb6, 0x3e2438ad, 0x3e25c58c, 0x3e275251,
	0x3e28defc, 0x3e2a6b8d, 0x3e2bf804, 0x3e2d8461, 0x3e2f10a2, 0x3e309cc9,
	0x3e3228d4, 0x3e33b4c4, 0x3e354098, 0x3e36cc50, 0x3e3857ec, 0x3e39e36c,
	0x3e3b6ecf, 0x3e3cfa15, 0x3e3e853e, 0x3e401049, 0x3e419b37, 0x3e432607,
	0x3e44b0b9, 0x3e463b4d, 0x3e47c5c2, 0x3e495018, 0x3e4ada4f, 0x3e4c6467,
	0x3e4dee60, 0x3e4f7838, 0x3e5101f1, 0x3e528b89, 0x3e541501, 0x3e559e58,
	0x3e57278f, 0x3e58b0a4, 0x3e5a3997, 0x3e5bc26a, 0x3e5d4b1a, 0x3e5ed3a8,
	0x3e605c13, 0x3e61e45c, 0x3e636c83, 0x3e64f486, 0x3e667c66, 0x3e680422,
	0x3e698bba, 0x3e6b132f, 0x3e6c9a7f, 0x3e6e21ab, 0x3e6fa8b2, 0x3e712f94,
	0x3e72b651, 0x3e743ce8, 0x3e75c35a, 0x3e7749a6, 0x3e78cfcc, 0x3e7a55cb,
	0x3e7bdba4, 0x3e7d6156, 0x3e7ee6e1, 0x3e803622, 0x3e80f8c0, 0x3e81bb4a,
	0x3e827dc0, 0x3e834022, 0x3e840270, 0x3e84c4aa, 0x3e8586ce, 0x3e8648df,
	0x3e870ada, 0x3e87ccc1, 0x3e888e93, 0x3e895050, 0x3e8a11f7, 0x3e8ad38a,
	0x3e8b9507, 0x3e8c566e, 0x3e8d17c0, 0x3e8dd8fc, 0x3e8e9a22, 0x3e8f5b32,
	0x3e901c2c, 0x3e90dd10, 0x3e919ddd, 0x3e925e94, 0x3e931f35, 0x3e93dfbf,
	0x3e94a031, 0x3e95608d, 0x3e9620d2, 0x3e96e100, 0x3e97a117, 0x3e986116,
	0x3e9920fe, 0x3e99e0ce, 0x3e9aa086, 0x3e9b6027, 0x3e9c1faf, 0x3e9cdf20,
	0x3e9d9e78, 0x3e9e5db8, 0x3e9f1cdf, 0x3e9fdbee, 0x3ea09ae5, 0x3ea159c2,
	0x3ea21887, 0x3ea2d733, 0x3ea395c5, 0x3ea4543f, 0x3ea5129f, 0x3ea5d0e5,
	0x3ea68f12, 0x3ea74d25, 0x3ea80b1f, 0x3ea8c8fe, 0x3ea986c4, 0x3eaa446f,
	0x3eab0201, 0x3eabbf77, 0x3eac7cd4, 0x3ead3a15, 0x3eadf73c, 0x3eaeb449,
	0x3eaf713a, 0x3eb02e10, 0x3eb0eacb, 0x3eb1a76b, 0x3eb263ef, 0x3eb32058,
	0x3eb3dca5, 0x3eb498d6, 0x3eb554ec, 0x3eb610e6, 0x3eb6ccc3, 0x3eb78884,
	0x3eb8442a, 0x3eb8ffb2, 0x3eb9bb1e, 0x3eba766e, 0x3ebb31a0, 0x3ebbecb6,
	0x3ebca7af, 0x3ebd628b, 0x3ebe1d4a, 0x3ebed7eb, 0x3ebf926f, 0x3ec04cd5,
	0x3ec1071e, 0x3ec1c148, 0x3ec27b55, 0x3ec33544, 0x3ec3ef15, 0x3ec4a8c8,
	0x3ec5625c, 0x3ec61bd2, 0x3ec6d529, 0x3ec78e62, 0x3ec8477c, 0x3ec90077,
	0x3ec9b953, 0x3eca7210, 0x3ecb2aae, 0x3ecbe32c, 0x3ecc9b8b, 0x3ecd53ca,
	0x3ece0bea, 0x3ecec3ea, 0x3ecf7bca, 0x3ed0338a, 0x3ed0eb2a, 0x3ed1a2aa,
	0x3ed25a09, 0x3ed31148, 0x3ed3c867, 0x3ed47f64, 0x3ed53641, 0x3ed5ecfd,
	0x3ed6a399, 0x3ed75a13, 0x3ed8106b, 0x3ed8c6a3, 0x3ed97cb9, 0x3eda32ad,
	0x3edae880, 0x3edb9e31, 0x3edc53c1, 0x3edd092e, 0x3eddbe79, 0x3ede73a2,
	0x3edf28a9, 0x3edfdd8d, 0x3ee0924f, 0x3ee146ee, 0x3ee1fb6a, 0x3ee2afc4,
	0x3ee363fa, 0x3ee4180e, 0x3ee4cbfe, 0x3ee57fcb, 0x3ee63375, 0x3ee6e6fb,
	0x3ee79a5d, 0x3ee84d9c, 0x3ee900b7, 0x3ee9b3ae, 0x3eea6681, 0x3eeb1930,
	0x3eebcbbb, 0x3eec7e21, 0x3eed3063, 0x3eede280, 0x3eee9479, 0x3eef464c,
	0x3eeff7fb, 0x3ef0a985, 0x3ef15aea, 0x3ef20c29, 0x3ef2bd43, 0x3ef36e38,
	0x3ef41f07, 0x3ef4cfb1, 0x3ef58035, 0x3ef63093, 0x3ef6e0cb, 0x3ef790dc,
	0x3ef840c8, 0x3ef8f08e, 0x3ef9a02d, 0x3efa4fa5, 0x3efafef7, 0x3efbae22,
	0x3efc5d27, 0x3efd0c04, 0x3efdbabb, 0x3efe694a, 0x3eff17b2, 0x3effc5f3,
	0x3f003a06, 0x3f0090ff, 0x3f00e7e4, 0x3f013eb5, 0x3f019573, 0x3f01ec1c,
	0x3f0242b1, 0x3f029932, 0x3f02ef9f, 0x3f0345f8, 0x3f039c3d, 0x3f03f26d,
	0x3f044889, 0x3f049e91, 0x3f04f484, 0x3f054a62, 0x3f05a02c, 0x3f05f5e2,
	0x3f064b82, 0x3f06a10e, 0x3f06f686, 0x3f074be8, 0x3f07a136, 0x3f07f66f,
	0x3f084b92, 0x3f08a0a1, 0x3f08f59b, 0x3f094a7f, 0x3f099f4e, 0x3f09f409,
	0x3f0a48ad, 0x3f0a9d3d, 0x3f0af1b7, 0x3f0b461c, 0x3f0b9a6b, 0x3f0beea5,
	0x3f0c42c9, 0x3f0c96d7, 0x3f0cead0, 0x3f0d3eb3, 0x3f0d9281, 0x3f0de638,
	0x3f0e39da, 0x3f0e8d65, 0x3f0ee0db, 0x3f0f343b, 0x3f0f8784, 0x3f0fdab8,
	0x3f102dd5, 0x3f1080dc, 0x3f10d3cd, 0x3f1126a7, 0x3f11796b, 0x3f11cc19,
	0x3f121eb0, 0x3f127130, 0x3f12c39a, 0x3f1315ee, 0x3f13682a, 0x3f13ba50,
	0x3f140c5f, 0x3f145e58, 0x3f14b039, 0x3f150204, 0x3f1553b7, 0x3f15a554,
	0x3f15f6d9, 0x3f164847, 0x3f16999f, 0x3f16eade, 0x3f173c07, 0x3f178d18,
	0x3f17de12, 0x3f182ef5, 0x3f187fc0, 0x3f18d073, 0x3f19210f, 0x3f197194,
	0x3f19c200, 0x3f1a1255, 0x3f1a6293, 0x3f1ab2b8, 0x3f1b02c6, 0x3f1b52bb,
	0x3f1ba299, 0x3f1bf25f, 0x3f1c420c, 0x3f1c91a2, 0x3f1ce11f, 0x3f1d3084,
	0x3f1d7fd1, 0x3f1dcf06, 0x3f1e1e22, 0x3f1e6d26, 0x3f1ebc12, 0x3f1f0ae5,
	0x3f1f599f, 0x3f1fa841, 0x3f1ff6cb, 0x3f20453b, 0x3f209393, 0x3f20e1d2,
	0x3f212ff9, 0x3f217e06, 0x3f21cbfb, 0x3f2219d7, 0x3f226799, 0x3f22b543,
	0x3f2302d3, 0x3f23504b, 0x3f239da9, 0x3f23eaee, 0x3f24381a, 0x3f24852c,
	0x3f24d225, 0x3f251f04, 0x3f256bcb, 0x3f25b877, 0x3f26050a, 0x3f265184,
	0x3f269de3, 0x3f26ea2a, 0x3f273656, 0x3f278268, 0x3f27ce61, 0x3f281a40,
	0x3f286605, 0x3f28b1b0, 0x3f28fd41, 0x3f2948b8, 0x3f299415, 0x3f29df57,
	0x3f2a2a80, 0x3f2a758e, 0x3f2ac082, 0x3f2b0b5b, 0x3f2b561b, 0x3f2ba0bf,
	0x3f2beb4a, 0x3f2c35b9, 0x3f2c800f, 0x3f2cca49, 0x3f2d1469, 0x3f2d5e6f,
	0x3f2da859, 0x3f2df229, 0x3f2e3bde, 0x3f2e8578, 0x3f2ecef7, 0x3f2f185b,
	0x3f2f61a5, 0x3f2faad3, 0x3f2ff3e6, 0x3f303cde, 0x3f3085bb, 0x3f30ce7c,
	0x3f311722, 0x3f315fad, 0x3f31a81d, 0x3f31f071, 0x3f3238aa, 0x3f3280c7,
	0x3f32c8c9, 0x3f3310af, 0x3f33587a, 0x3f33a029, 0x3f33e7bc, 0x3f342f34,
	0x3f34768f, 0x3f34bdcf, 0x3f3504f3, 0x3f354bfb, 0x3f3592e7, 0x3f35d9b8,
	0x3f36206c, 0x3f366704, 0x3f36ad7f, 0x3f36f3df, 0x3f373a23, 0x3f37804a,
	0x3f37c655, 0x3f380c43, 0x3f385216, 0x3f3897cb, 0x3f38dd65, 0x3f3922e1,
	0x3f396842, 0x3f39ad85, 0x3f39f2ac, 0x3f3a37b7, 0x3f3a7ca4, 0x3f3ac175,
	0x3f3b0629, 0x3f3b4ac1, 0x3f3b8f3b, 0x3f3bd398, 0x3f3c17d9, 0x3f3c5bfc,
	0x3f3ca003, 0x3f3ce3ec, 0x3f3d27b8, 0x3f3d6b67, 0x3f3daef9, 0x3f3df26e,
	0x3f3e35c5, 0x3f3e78ff, 0x3f3ebc1b, 0x3f3eff1b, 0x3f3f41fc, 0x3f3f84c0,
	0x3f3fc767, 0x3f4009f0, 0x3f404c5c, 0x3f408ea9, 0x3f40d0da, 0x3f4112ec,
	0x3f4154e1, 0x3f4196b7, 0x3f41d870, 0x3f421a0b, 0x3f425b89, 0x3f429ce8,
	0x3f42de29, 0x3f431f4c, 0x3f436051, 0x3f43a138, 0x3f43e200, 0x3f4422ab,
	0x3f446337, 0x3f44a3a5, 0x3f44e3f5, 0x3f452426, 0x3f456439, 0x3f45a42d,
	0x3f45e403, 0x3f4623bb, 0x3f466354, 0x3f46a2ce, 0x3f46e22a, 0x3f472167,
	0x3f476085, 0x3f479f84, 0x3f47de65, 0x3f481d27, 0x3f485bca, 0x3f489a4e,
	0x3f48d8b3, 0x3f4916fa, 0x3f495521, 0x3f499329, 0x3f49d112, 0x3f4a0edc,
	0x3f4a4c87, 0x3f4a8a13, 0x3f4ac77f, 0x3f4b04cc, 0x3f4b41fa, 0x3f4b7f09,
	0x3f4bbbf8, 0x3f4bf8c7, 0x3f4c3578, 0x3f4c7208, 0x3f4cae79, 0x3f4ceacb,
	0x3f4d26fd, 0x3f4d6310, 0x3f4d9f02, 0x3f4ddad5, 0x3f4e1689, 0x3f4e521c,
	0x3f4e8d90, 0x3f4ec8e4, 0x3f4f0417, 0x3f4f3f2b, 0x3f4f7a1f, 0x3f4fb4f4,
	0x3f4fefa8, 0x3f502a3b, 0x3f5064af, 0x3f509f03, 0x3f50d937, 0x3f51134a,
	0x3f514d3d, 0x3f518710, 0x3f51c0c2, 0x3f51fa54, 0x3f5233c6, 0x3f526d18,
	0x3f52a649, 0x3f52df59, 0x3f531849, 0x3f535118, 0x3f5389c7, 0x3f53c255,
	0x3f53fac3, 0x3f54330f, 0x3f546b3b, 0x3f54a347, 0x3f54db31, 0x3f5512fb,
	0x3f554aa4, 0x3f55822c, 0x3f55b993, 0x3f55f0d9, 0x3f5627fe, 0x3f565f02,
	0x3f5695e5, 0x3f56cca7, 0x3f570348, 0x3f5739c7, 0x3f577026, 0x3f57a663,
	0x3f57dc7f, 0x3f581279, 0x3f584853, 0x3f587e0b, 0x3f58b3a1, 0x3f58e916,
	0x3f591e6a, 0x3f59539c, 0x3f5988ad, 0x3f59bd9c, 0x3f59f26a, 0x3f5a2716,
	0x3f5a5ba0, 0x3f5a9009, 0x3f5ac450, 0x3f5af875, 0x3f5b2c79, 0x3f5b605a,
	0x3f5b941a, 0x3f5bc7b8, 0x3f5bfb34, 0x3f5c2e8e, 0x3f5c61c7, 0x3f5c94dd,
	0x3f5cc7d1, 0x3f5cfaa3, 0x3f5d2d53, 0x3f5d5fe1, 0x3f5d924d, 0x3f5dc497,
	0x3f5df6be, 0x3f5e28c3, 0x3f5e5aa6, 0x3f5e8c67, 0x3f5ebe05, 0x3f5eef81,
	0x3f5f20db, 0x3f5f5212, 0x3f5f8327, 0x3f5fb419, 0x3f5fe4e9, 0x3f601596,
	0x3f604621, 0x3f607689, 0x3f60a6cf, 0x3f60d6f2, 0x3f6106f2, 0x3f6136d0,
	0x3f61668a, 0x3f619622, 0x3f61c598, 0x3f61f4ea, 0x3f62241a, 0x3f625326,
	0x3f628210, 0x3f62b0d7, 0x3f62df7b, 0x3f630dfc, 0x3f633c5a, 0x3f636a95,
	0x3f6398ac, 0x3f63c6a1, 0x3f63f473, 0x3f642221, 0x3f644fac, 0x3f647d14,
	0x3f64aa59, 0x3f64d77b, 0x3f650479, 0x3f653154, 0x3f655e0b, 0x3f658aa0,
	0x3f65b710, 0x3f65e35e, 0x3f660f88, 0x3f663b8e, 0x3f666771, 0x3f669330,
	0x3f66becc, 0x3f66ea45, 0x3f671599, 0x3f6740ca, 0x3f676bd8, 0x3f6796c1,
	0x3f67c187, 0x3f67ec29, 0x3f6816a8, 0x3f684103, 0x3f686b39, 0x3f68954c,
	0x3f68bf3c, 0x3f68e907, 0x3f6912ae, 0x3f693c32, 0x3f696591, 0x3f698ecc,
	0x3f69b7e4, 0x3f69e0d7, 0x3f6a09a7, 0x3f6a3252, 0x3f6a5ad9, 0x3f6a833c,
	0x3f6aab7b, 0x3f6ad395, 0x3f6afb8c, 0x3f6b235e, 0x3f6b4b0c, 0x3f6b7295,
	0x3f6b99fb, 0x3f6bc13b, 0x3f6be858, 0x3f6c0f50, 0x3f6c3624, 0x3f6c5cd4,
	0x3f6c835e, 0x3f6ca9c5, 0x3f6cd007, 0x3f6cf624, 0x3f6d1c1d, 0x3f6d41f2,
	0x3f6d67a1, 0x3f6d8d2d, 0x3f6db293, 0x3f6dd7d5, 0x3f6dfcf2, 0x3f6e21eb,
	0x3f6e46be, 0x3f6e6b6d, 0x3f6e8ff8, 0x3f6eb45d, 0x3f6ed89e, 0x3f6efcba,
	0x3f6f20b0, 0x3f6f4483, 0x3f6f6830, 0x3f6f8bb8, 0x3f6faf1b, 0x3f6fd25a,
	0x3f6ff573, 0x3f701867, 0x3f703b37, 0x3f705de1, 0x3f708066, 0x3f70a2c6,
	0x3f70c501, 0x3f70e717, 0x3f710908, 0x3f712ad4, 0x3f714c7a, 0x3f716dfb,
	0x3f718f57, 0x3f71b08e, 0x3f71d19f, 0x3f71f28c, 0x3f721352, 0x3f7233f4,
	0x3f725470, 0x3f7274c7, 0x3f7294f8, 0x3f72b504, 0x3f72d4eb, 0x3f72f4ac,
	0x3f731447, 0x3f7333be, 0x3f73530e, 0x3f737239, 0x3f73913f, 0x3f73b01f,
	0x3f73ced9, 0x3f73ed6e, 0x3f740bdd, 0x3f742a27, 0x3f74484b, 0x3f746649,
	0x3f748422, 0x3f74a1d5, 0x3f74bf62, 0x3f74dcc9, 0x3f74fa0b, 0x3f751727,
	0x3f75341d, 0x3f7550ed, 0x3f756d97, 0x3f758a1c, 0x3f75a67b, 0x3f75c2b3,
	0x3f75dec6, 0x3f75fab3, 0x3f76167a, 0x3f76321b, 0x3f764d97, 0x3f7668ec,
	0x3f76841b, 0x3f769f24, 0x3f76ba07, 0x3f76d4c4, 0x3f76ef5b, 0x3f7709cc,
	0x3f772417, 0x3f773e3c, 0x3f77583a, 0x3f777213, 0x3f778bc5, 0x3f77a551,
	0x3f77beb7, 0x3f77d7f7, 0x3f77f110, 0x3f780a04, 0x3f7822d1, 0x3f783b77,
	0x3f7853f8, 0x3f786c52, 0x3f788486, 0x3f789c93, 0x3f78b47b, 0x3f78cc3b,
	0x3f78e3d6, 0x3f78fb4a, 0x3f791298, 0x3f7929bf, 0x3f7940c0, 0x3f79579a,
	0x3f796e4e, 0x3f7984dc, 0x3f799b43, 0x3f79b183, 0x3f79c79d, 0x3f79dd91,
	0x3f79f35e, 0x3f7a0904, 0x3f7a1e84, 0x3f7a33dd, 0x3f7a4910, 0x3f7a5e1c,
	0x3f7a7302, 0x3f7a87c1, 0x3f7a9c59, 0x3f7ab0cb, 0x3f7ac516, 0x3f7ad93a,
	0x3f7aed37, 0x3f7b010e, 0x3f7b14be, 0x3f7b2848, 0x3f7b3bab, 0x3f7b4ee7,
	0x3f7b61fc, 0x3f7b74ea, 0x3f7b87b2, 0x3f7b9a53, 0x3f7baccd, 0x3f7bbf20,
	0x3f7bd14d, 0x3f7be353, 0x3f7bf531, 0x3f7c06e9, 0x3f7c187a, 0x3f7c29e5,
	0x3f7c3b28, 0x3f7c4c44, 0x3f7c5d3a, 0x3f7c6e08, 0x3f7c7eb0, 0x3f7c8f31,
	0x3f7c9f8a, 0x3f7cafbd, 0x3f7cbfc9, 0x3f7ccfae, 0x3f7cdf6c, 0x3f7cef03,
	0x3f7cfe73, 0x3f7d0dbc, 0x3f7d1cdd, 0x3f7d2bd8, 0x3f7d3aac, 0x3f7d4959,
	0x3f7d57de, 0x3f7d663d, 0x3f7d7474, 0x3f7d8285, 0x3f7d906e, 0x3f7d9e30,
	0x3f7dabcc, 0x3f7db940, 0x3f7dc68c, 0x3f7dd3b2, 0x3f7de0b1, 0x3f7ded88,
	0x3f7dfa38, 0x3f7e06c2, 0x3f7e1324, 0x3f7e1f5e, 0x3f7e2b72, 0x3f7e375e,
	0x3f7e4323, 0x3f7e4ec1, 0x3f7e5a38, 0x3f7e6588, 0x3f7e70b0, 0x3f7e7bb1,
	0x3f7e868b, 0x3f7e913d, 0x3f7e9bc9, 0x3f7ea62d, 0x3f7eb069, 0x3f7eba7f,
	0x3f7ec46d, 0x3f7ece34, 0x3f7ed7d4, 0x3f7ee14c, 0x3f7eea9d, 0x3f7ef3c7,
	0x3f7efcc9, 0x3f7f05a4, 0x3f7f0e58, 0x3f7f16e4, 0x3f7f1f49, 0x3f7f2787,
	0x3f7f2f9d, 0x3f7f378c, 0x3f7f3f54, 0x3f7f46f4, 0x3f7f4e6d, 0x3f7f55bf,
	0x3f7f5ce9, 0x3f7f63ec, 0x3f7f6ac7, 0x3f7f717b, 0x3f7f7808, 0x3f7f7e6d,
	0x3f7f84ab, 0x3f7f8ac2, 0x3f7f90b1, 0x3f7f9678, 0x3f7f9c18, 0x3f7fa191,
	0x3f7fa6e3, 0x3f7fac0d, 0x3f7fb10f, 0x3f7fb5ea, 0x3f7fba9e, 0x3f7fbf2a,
	0x3f7fc38f, 0x3f7fc7cc, 0x3f7fcbe2, 0x3f7fcfd1, 0x3f7fd397, 0x3f7fd737,
	0x3f7fdaaf, 0x3f7fde00, 0x3f7fe129, 0x3f7fe42b, 0x3f7fe705, 0x3f7fe9b8,
	0x3f7fec43, 0x3f7feea7, 0x3f7ff0e3, 0x3f7ff2f8, 0x3f7ff4e6, 0x3f7ff6ac,
	0x3f7ff84a, 0x3f7ff9c1, 0x3f7ffb11, 0x3f7ffc39, 0x3f7ffd39, 0x3f7ffe13,
	0x3f7ffec4, 0x3f7fff4e, 0x3f7fffb1, 0x3f7fffec
};

U32 CosTabU32[] = {
	0x3f800000, 0x3f7fffec, 0x3f7fffb1, 0x3f7fff4e, 0x3f7ffec4, 0x3f7ffe13,
	0x3f7ffd39, 0x3f7ffc39, 0x3f7ffb11, 0x3f7ff9c1, 0x3f7ff84a, 0x3f7ff6ac,
	0x3f7ff4e6, 0x3f7ff2f8, 0x3f7ff0e3, 0x3f7feea7, 0x3f7fec43, 0x3f7fe9b8,
	0x3f7fe705, 0x3f7fe42b, 0x3f7fe129, 0x3f7fde00, 0x3f7fdaaf, 0x3f7fd737,
	0x3f7fd397, 0x3f7fcfd1, 0x3f7fcbe2, 0x3f7fc7cc, 0x3f7fc38f, 0x3f7fbf2a,
	0x3f7fba9e, 0x3f7fb5ea, 0x3f7fb10f, 0x3f7fac0d, 0x3f7fa6e3, 0x3f7fa191,
	0x3f7f9c18, 0x3f7f9678, 0x3f7f90b1, 0x3f7f8ac2, 0x3f7f84ab, 0x3f7f7e6d,
	0x3f7f7808, 0x3f7f717b, 0x3f7f6ac7, 0x3f7f63ec, 0x3f7f5ce9, 0x3f7f55bf,
	0x3f7f4e6d, 0x3f7f46f4, 0x3f7f3f54, 0x3f7f378c, 0x3f7f2f9d, 0x3f7f2787,
	0x3f7f1f49, 0x3f7f16e4, 0x3f7f0e58, 0x3f7f05a4, 0x3f7efcc9, 0x3f7ef3c7,
	0x3f7eea9d, 0x3f7ee14c, 0x3f7ed7d4, 0x3f7ece34, 0x3f7ec46d, 0x3f7eba7f,
	0x3f7eb069, 0x3f7ea62d, 0x3f7e9bc9, 0x3f7e913d, 0x3f7e868b, 0x3f7e7bb1,
	0x3f7e70b0, 0x3f7e6588, 0x3f7e5a38, 0x3f7e4ec1, 0x3f7e4323, 0x3f7e375e,
	0x3f7e2b72, 0x3f7e1f5e, 0x3f7e1324, 0x3f7e06c2, 0x3f7dfa38, 0x3f7ded88,
	0x3f7de0b1, 0x3f7dd3b2, 0x3f7dc68c, 0x3f7db940, 0x3f7dabcc, 0x3f7d9e30,
	0x3f7d906e, 0x3f7d8285, 0x3f7d7474, 0x3f7d663d, 0x3f7d57de, 0x3f7d4959,
	0x3f7d3aac, 0x3f7d2bd8, 0x3f7d1cdd, 0x3f7d0dbc, 0x3f7cfe73, 0x3f7cef03,
	0x3f7cdf6c, 0x3f7ccfae, 0x3f7cbfc9, 0x3f7cafbd, 0x3f7c9f8a, 0x3f7c8f31,
	0x3f7c7eb0, 0x3f7c6e08, 0x3f7c5d3a, 0x3f7c4c44, 0x3f7c3b28, 0x3f7c29e5,
	0x3f7c187a, 0x3f7c06e9, 0x3f7bf531, 0x3f7be353, 0x3f7bd14d, 0x3f7bbf20,
	0x3f7baccd, 0x3f7b9a53, 0x3f7b87b2, 0x3f7b74ea, 0x3f7b61fc, 0x3f7b4ee7,
	0x3f7b3bab, 0x3f7b2848, 0x3f7b14be, 0x3f7b010e, 0x3f7aed37, 0x3f7ad93a,
	0x3f7ac516, 0x3f7ab0cb, 0x3f7a9c59, 0x3f7a87c1, 0x3f7a7302, 0x3f7a5e1c,
	0x3f7a4910, 0x3f7a33dd, 0x3f7a1e84, 0x3f7a0904, 0x3f79f35e, 0x3f79dd91,
	0x3f79c79d, 0x3f79b183, 0x3f799b43, 0x3f7984dc, 0x3f796e4e, 0x3f79579a,
	0x3f7940c0, 0x3f7929bf, 0x3f791298, 0x3f78fb4a, 0x3f78e3d6, 0x3f78cc3b,
	0x3f78b47b, 0x3f789c93, 0x3f788486, 0x3f786c52, 0x3f7853f8, 0x3f783b77,
	0x3f7822d1, 0x3f780a04, 0x3f77f110, 0x3f77d7f7, 0x3f77beb7, 0x3f77a551,
	0x3f778bc5, 0x3f777213, 0x3f77583a, 0x3f773e3c, 0x3f772417, 0x3f7709cc,
	0x3f76ef5b, 0x3f76d4c4, 0x3f76ba07, 0x3f769f24, 0x3f76841b, 0x3f7668ec,
	0x3f764d97, 0x3f76321b, 0x3f76167a, 0x3f75fab3, 0x3f75dec6, 0x3f75c2b3,
	0x3f75a67b, 0x3f758a1c, 0x3f756d97, 0x3f7550ed, 0x3f75341d, 0x3f751727,
	0x3f74fa0b, 0x3f74dcc9, 0x3f74bf62, 0x3f74a1d5, 0x3f748422, 0x3f746649,
	0x3f74484b, 0x3f742a27, 0x3f740bdd, 0x3f73ed6e, 0x3f73ced9, 0x3f73b01f,
	0x3f73913f, 0x3f737239, 0x3f73530e, 0x3f7333be, 0x3f731447, 0x3f72f4ac,
	0x3f72d4eb, 0x3f72b504, 0x3f7294f8, 0x3f7274c7, 0x3f725470, 0x3f7233f4,
	0x3f721352, 0x3f71f28c, 0x3f71d19f, 0x3f71b08e, 0x3f718f57, 0x3f716dfb,
	0x3f714c7a, 0x3f712ad4, 0x3f710908, 0x3f70e717, 0x3f70c501, 0x3f70a2c6,
	0x3f708066, 0x3f705de1, 0x3f703b37, 0x3f701867, 0x3f6ff573, 0x3f6fd25a,
	0x3f6faf1b, 0x3f6f8bb8, 0x3f6f6830, 0x3f6f4483, 0x3f6f20b0, 0x3f6efcba,
	0x3f6ed89e, 0x3f6eb45d, 0x3f6e8ff8, 0x3f6e6b6d, 0x3f6e46be, 0x3f6e21eb,
	0x3f6dfcf2, 0x3f6dd7d5, 0x3f6db293, 0x3f6d8d2d, 0x3f6d67a1, 0x3f6d41f2,
	0x3f6d1c1d, 0x3f6cf624, 0x3f6cd007, 0x3f6ca9c5, 0x3f6c835e, 0x3f6c5cd4,
	0x3f6c3624, 0x3f6c0f50, 0x3f6be858, 0x3f6bc13b, 0x3f6b99fb, 0x3f6b7295,
	0x3f6b4b0c, 0x3f6b235e, 0x3f6afb8c, 0x3f6ad395, 0x3f6aab7b, 0x3f6a833c,
	0x3f6a5ad9, 0x3f6a3252, 0x3f6a09a7, 0x3f69e0d7, 0x3f69b7e4, 0x3f698ecc,
	0x3f696591, 0x3f693c32, 0x3f6912ae, 0x3f68e907, 0x3f68bf3c, 0x3f68954c,
	0x3f686b39, 0x3f684103, 0x3f6816a8, 0x3f67ec29, 0x3f67c187, 0x3f6796c1,
	0x3f676bd8, 0x3f6740ca, 0x3f671599, 0x3f66ea45, 0x3f66becc, 0x3f669330,
	0x3f666771, 0x3f663b8e, 0x3f660f88, 0x3f65e35e, 0x3f65b710, 0x3f658aa0,
	0x3f655e0b, 0x3f653154, 0x3f650479, 0x3f64d77b, 0x3f64aa59, 0x3f647d14,
	0x3f644fac, 0x3f642221, 0x3f63f473, 0x3f63c6a1, 0x3f6398ac, 0x3f636a95,
	0x3f633c5a, 0x3f630dfc, 0x3f62df7b, 0x3f62b0d7, 0x3f628210, 0x3f625326,
	0x3f62241a, 0x3f61f4ea, 0x3f61c598, 0x3f619622, 0x3f61668a, 0x3f6136d0,
	0x3f6106f2, 0x3f60d6f2, 0x3f60a6cf, 0x3f607689, 0x3f604621, 0x3f601596,
	0x3f5fe4e9, 0x3f5fb419, 0x3f5f8327, 0x3f5f5212, 0x3f5f20db, 0x3f5eef81,
	0x3f5ebe05, 0x3f5e8c67, 0x3f5e5aa6, 0x3f5e28c3, 0x3f5df6be, 0x3f5dc497,
	0x3f5d924d, 0x3f5d5fe1, 0x3f5d2d53, 0x3f5cfaa3, 0x3f5cc7d1, 0x3f5c94dd,
	0x3f5c61c7, 0x3f5c2e8e, 0x3f5bfb34, 0x3f5bc7b8, 0x3f5b941a, 0x3f5b605a,
	0x3f5b2c79, 0x3f5af875, 0x3f5ac450, 0x3f5a9009, 0x3f5a5ba0, 0x3f5a2716,
	0x3f59f26a, 0x3f59bd9c, 0x3f5988ad, 0x3f59539c, 0x3f591e6a, 0x3f58e916,
	0x3f58b3a1, 0x3f587e0b, 0x3f584853, 0x3f581279, 0x3f57dc7f, 0x3f57a663,
	0x3f577026, 0x3f5739c7, 0x3f570348, 0x3f56cca7, 0x3f5695e5, 0x3f565f02,
	0x3f5627fe, 0x3f55f0d9, 0x3f55b993, 0x3f55822c, 0x3f554aa4, 0x3f5512fb,
	0x3f54db31, 0x3f54a347, 0x3f546b3b, 0x3f54330f, 0x3f53fac3, 0x3f53c255,
	0x3f5389c7, 0x3f535118, 0x3f531849, 0x3f52df59, 0x3f52a649, 0x3f526d18,
	0x3f5233c6, 0x3f51fa54, 0x3f51c0c2, 0x3f518710, 0x3f514d3d, 0x3f51134a,
	0x3f50d937, 0x3f509f03, 0x3f5064af, 0x3f502a3b, 0x3f4fefa8, 0x3f4fb4f4,
	0x3f4f7a1f, 0x3f4f3f2b, 0x3f4f0417, 0x3f4ec8e4, 0x3f4e8d90, 0x3f4e521c,
	0x3f4e1689, 0x3f4ddad5, 0x3f4d9f02, 0x3f4d6310, 0x3f4d26fd, 0x3f4ceacb,
	0x3f4cae79, 0x3f4c7208, 0x3f4c3578, 0x3f4bf8c7, 0x3f4bbbf8, 0x3f4b7f09,
	0x3f4b41fa, 0x3f4b04cc, 0x3f4ac77f, 0x3f4a8a13, 0x3f4a4c87, 0x3f4a0edc,
	0x3f49d112, 0x3f499329, 0x3f495521, 0x3f4916fa, 0x3f48d8b3, 0x3f489a4e,
	0x3f485bca, 0x3f481d27, 0x3f47de65, 0x3f479f84, 0x3f476085, 0x3f472167,
	0x3f46e22a, 0x3f46a2ce, 0x3f466354, 0x3f4623bb, 0x3f45e403, 0x3f45a42d,
	0x3f456439, 0x3f452426, 0x3f44e3f5, 0x3f44a3a5, 0x3f446337, 0x3f4422ab,
	0x3f43e200, 0x3f43a138, 0x3f436051, 0x3f431f4c, 0x3f42de29, 0x3f429ce8,
	0x3f425b89, 0x3f421a0b, 0x3f41d870, 0x3f4196b7, 0x3f4154e1, 0x3f4112ec,
	0x3f40d0da, 0x3f408ea9, 0x3f404c5c, 0x3f4009f0, 0x3f3fc767, 0x3f3f84c0,
	0x3f3f41fc, 0x3f3eff1b, 0x3f3ebc1b, 0x3f3e78ff, 0x3f3e35c5, 0x3f3df26e,
	0x3f3daef9, 0x3f3d6b67, 0x3f3d27b8, 0x3f3ce3ec, 0x3f3ca003, 0x3f3c5bfc,
	0x3f3c17d9, 0x3f3bd398, 0x3f3b8f3b, 0x3f3b4ac1, 0x3f3b0629, 0x3f3ac175,
	0x3f3a7ca4, 0x3f3a37b7, 0x3f39f2ac, 0x3f39ad85, 0x3f396842, 0x3f3922e1,
	0x3f38dd65, 0x3f3897cb, 0x3f385216, 0x3f380c43, 0x3f37c655, 0x3f37804a,
	0x3f373a23, 0x3f36f3df, 0x3f36ad7f, 0x3f366704, 0x3f36206c, 0x3f35d9b8,
	0x3f3592e7, 0x3f354bfb, 0x3f3504f3, 0x3f34bdcf, 0x3f34768f, 0x3f342f34,
	0x3f33e7bc, 0x3f33a029, 0x3f33587a, 0x3f3310af, 0x3f32c8c9, 0x3f3280c7,
	0x3f3238aa, 0x3f31f071, 0x3f31a81d, 0x3f315fad, 0x3f311722, 0x3f30ce7c,
	0x3f3085bb, 0x3f303cde, 0x3f2ff3e6, 0x3f2faad3, 0x3f2f61a5, 0x3f2f185b,
	0x3f2ecef7, 0x3f2e8578, 0x3f2e3bde, 0x3f2df229, 0x3f2da859, 0x3f2d5e6f,
	0x3f2d1469, 0x3f2cca49, 0x3f2c800f, 0x3f2c35b9, 0x3f2beb4a, 0x3f2ba0bf,
	0x3f2b561b, 0x3f2b0b5b, 0x3f2ac082, 0x3f2a758e, 0x3f2a2a80, 0x3f29df57,
	0x3f299415, 0x3f2948b8, 0x3f28fd41, 0x3f28b1b0, 0x3f286605, 0x3f281a40,
	0x3f27ce61, 0x3f278268, 0x3f273656, 0x3f26ea2a, 0x3f269de3, 0x3f265184,
	0x3f26050a, 0x3f25b877, 0x3f256bcb, 0x3f251f04, 0x3f24d225, 0x3f24852c,
	0x3f24381a, 0x3f23eaee, 0x3f239da9, 0x3f23504b, 0x3f2302d3, 0x3f22b543,
	0x3f226799, 0x3f2219d7, 0x3f21cbfb, 0x3f217e06, 0x3f212ff9, 0x3f20e1d2,
	0x3f209393, 0x3f20453b, 0x3f1ff6cb, 0x3f1fa841, 0x3f1f599f, 0x3f1f0ae5,
	0x3f1ebc12, 0x3f1e6d26, 0x3f1e1e22, 0x3f1dcf06, 0x3f1d7fd1, 0x3f1d3084,
	0x3f1ce11f, 0x3f1c91a2, 0x3f1c420c, 0x3f1bf25f, 0x3f1ba299, 0x3f1b52bb,
	0x3f1b02c6, 0x3f1ab2b8, 0x3f1a6293, 0x3f1a1255, 0x3f19c200, 0x3f197194,
	0x3f19210f, 0x3f18d073, 0x3f187fc0, 0x3f182ef5, 0x3f17de12, 0x3f178d18,
	0x3f173c07, 0x3f16eade, 0x3f16999f, 0x3f164847, 0x3f15f6d9, 0x3f15a554,
	0x3f1553b7, 0x3f150204, 0x3f14b039, 0x3f145e58, 0x3f140c5f, 0x3f13ba50,
	0x3f13682a, 0x3f1315ee, 0x3f12c39a, 0x3f127130, 0x3f121eb0, 0x3f11cc19,
	0x3f11796b, 0x3f1126a7, 0x3f10d3cd, 0x3f1080dc, 0x3f102dd5, 0x3f0fdab8,
	0x3f0f8784, 0x3f0f343b, 0x3f0ee0db, 0x3f0e8d65, 0x3f0e39da, 0x3f0de638,
	0x3f0d9281, 0x3f0d3eb3, 0x3f0cead0, 0x3f0c96d7, 0x3f0c42c9, 0x3f0beea5,
	0x3f0b9a6b, 0x3f0b461c, 0x3f0af1b7, 0x3f0a9d3d, 0x3f0a48ad, 0x3f09f409,
	0x3f099f4e, 0x3f094a7f, 0x3f08f59b, 0x3f08a0a1, 0x3f084b92, 0x3f07f66f,
	0x3f07a136, 0x3f074be8, 0x3f06f686, 0x3f06a10e, 0x3f064b82, 0x3f05f5e2,
	0x3f05a02c, 0x3f054a62, 0x3f04f484, 0x3f049e91, 0x3f044889, 0x3f03f26d,
	0x3f039c3d, 0x3f0345f8, 0x3f02ef9f, 0x3f029932, 0x3f0242b1, 0x3f01ec1c,
	0x3f019573, 0x3f013eb5, 0x3f00e7e4, 0x3f0090ff, 0x3f003a06, 0x3effc5f3,
	0x3eff17b2, 0x3efe694a, 0x3efdbabb, 0x3efd0c04, 0x3efc5d27, 0x3efbae22,
	0x3efafef7, 0x3efa4fa5, 0x3ef9a02d, 0x3ef8f08e, 0x3ef840c8, 0x3ef790dc,
	0x3ef6e0cb, 0x3ef63093, 0x3ef58035, 0x3ef4cfb1, 0x3ef41f07, 0x3ef36e38,
	0x3ef2bd43, 0x3ef20c29, 0x3ef15aea, 0x3ef0a985, 0x3eeff7fb, 0x3eef464c,
	0x3eee9479, 0x3eede280, 0x3eed3063, 0x3eec7e21, 0x3eebcbbb, 0x3eeb1930,
	0x3eea6681, 0x3ee9b3ae, 0x3ee900b7, 0x3ee84d9c, 0x3ee79a5d, 0x3ee6e6fb,
	0x3ee63375, 0x3ee57fcb, 0x3ee4cbfe, 0x3ee4180e, 0x3ee363fa, 0x3ee2afc4,
	0x3ee1fb6a, 0x3ee146ee, 0x3ee0924f, 0x3edfdd8d, 0x3edf28a9, 0x3ede73a2,
	0x3eddbe79, 0x3edd092e, 0x3edc53c1, 0x3edb9e31, 0x3edae880, 0x3eda32ad,
	0x3ed97cb9, 0x3ed8c6a3, 0x3ed8106b, 0x3ed75a13, 0x3ed6a399, 0x3ed5ecfd,
	0x3ed53641, 0x3ed47f64, 0x3ed3c867, 0x3ed31148, 0x3ed25a09, 0x3ed1a2aa,
	0x3ed0eb2a, 0x3ed0338a, 0x3ecf7bca, 0x3ecec3ea, 0x3ece0bea, 0x3ecd53ca,
	0x3ecc9b8b, 0x3ecbe32c, 0x3ecb2aae, 0x3eca7210, 0x3ec9b953, 0x3ec90077,
	0x3ec8477c, 0x3ec78e62, 0x3ec6d529, 0x3ec61bd2, 0x3ec5625c, 0x3ec4a8c8,
	0x3ec3ef15, 0x3ec33544, 0x3ec27b55, 0x3ec1c148, 0x3ec1071e, 0x3ec04cd5,
	0x3ebf926f, 0x3ebed7eb, 0x3ebe1d4a, 0x3ebd628b, 0x3ebca7af, 0x3ebbecb6,
	0x3ebb31a0, 0x3eba766e, 0x3eb9bb1e, 0x3eb8ffb2, 0x3eb8442a, 0x3eb78884,
	0x3eb6ccc3, 0x3eb610e6, 0x3eb554ec, 0x3eb498d6, 0x3eb3dca5, 0x3eb32058,
	0x3eb263ef, 0x3eb1a76b, 0x3eb0eacb, 0x3eb02e10, 0x3eaf713a, 0x3eaeb449,
	0x3eadf73c, 0x3ead3a15, 0x3eac7cd4, 0x3eabbf77, 0x3eab0201, 0x3eaa446f,
	0x3ea986c4, 0x3ea8c8fe, 0x3ea80b1f, 0x3ea74d25, 0x3ea68f12, 0x3ea5d0e5,
	0x3ea5129f, 0x3ea4543f, 0x3ea395c5, 0x3ea2d733, 0x3ea21887, 0x3ea159c2,
	0x3ea09ae5, 0x3e9fdbee, 0x3e9f1cdf, 0x3e9e5db8, 0x3e9d9e78, 0x3e9cdf20,
	0x3e9c1faf, 0x3e9b6027, 0x3e9aa086, 0x3e99e0ce, 0x3e9920fe, 0x3e986116,
	0x3e97a117, 0x3e96e100, 0x3e9620d2, 0x3e95608d, 0x3e94a031, 0x3e93dfbf,
	0x3e931f35, 0x3e925e94, 0x3e919ddd, 0x3e90dd10, 0x3e901c2c, 0x3e8f5b32,
	0x3e8e9a22, 0x3e8dd8fc, 0x3e8d17c0, 0x3e8c566e, 0x3e8b9507, 0x3e8ad38a,
	0x3e8a11f7, 0x3e895050, 0x3e888e93, 0x3e87ccc1, 0x3e870ada, 0x3e8648df,
	0x3e8586ce, 0x3e84c4aa, 0x3e840270, 0x3e834022, 0x3e827dc0, 0x3e81bb4a,
	0x3e80f8c0, 0x3e803622, 0x3e7ee6e1, 0x3e7d6156, 0x3e7bdba4, 0x3e7a55cb,
	0x3e78cfcc, 0x3e7749a6, 0x3e75c35a, 0x3e743ce8, 0x3e72b651, 0x3e712f94,
	0x3e6fa8b2, 0x3e6e21ab, 0x3e6c9a7f, 0x3e6b132f, 0x3e698bba, 0x3e680422,
	0x3e667c66, 0x3e64f486, 0x3e636c83, 0x3e61e45c, 0x3e605c13, 0x3e5ed3a8,
	0x3e5d4b1a, 0x3e5bc26a, 0x3e5a3997, 0x3e58b0a4, 0x3e57278f, 0x3e559e58,
	0x3e541501, 0x3e528b89, 0x3e5101f1, 0x3e4f7838, 0x3e4dee60, 0x3e4c6467,
	0x3e4ada4f, 0x3e495018, 0x3e47c5c2, 0x3e463b4d, 0x3e44b0b9, 0x3e432607,
	0x3e419b37, 0x3e401049, 0x3e3e853e, 0x3e3cfa15, 0x3e3b6ecf, 0x3e39e36c,
	0x3e3857ec, 0x3e36cc50, 0x3e354098, 0x3e33b4c4, 0x3e3228d4, 0x3e309cc9,
	0x3e2f10a2, 0x3e2d8461, 0x3e2bf804, 0x3e2a6b8d, 0x3e28defc, 0x3e275251,
	0x3e25c58c, 0x3e2438ad, 0x3e22abb6, 0x3e211ea5, 0x3e1f917b, 0x3e1e0438,
	0x3e1c76de, 0x3e1ae96b, 0x3e195be0, 0x3e17ce3d, 0x3e164083, 0x3e14b2b2,
	0x3e1324ca, 0x3e1196cc, 0x3e1008b7, 0x3e0e7a8b, 0x3e0cec4a, 0x3e0b5df3,
	0x3e09cf86, 0x3e084105, 0x3e06b26e, 0x3e0523c2, 0x3e039502, 0x3e02062e,
	0x3e007745, 0x3dfdd092, 0x3dfab273, 0x3df7942c, 0x3df475c0, 0x3df1572e,
	0x3dee3876, 0x3deb199a, 0x3de7fa9a, 0x3de4db76, 0x3de1bc2e, 0x3dde9cc4,
	0x3ddb7d37, 0x3dd85d89, 0x3dd53db9, 0x3dd21dc8, 0x3dcefdb7, 0x3dcbdd86,
	0x3dc8bd36, 0x3dc59cc6, 0x3dc27c39, 0x3dbf5b8d, 0x3dbc3ac3, 0x3db919dd,
	0x3db5f8da, 0x3db2d7bb, 0x3dafb680, 0x3dac952b, 0x3da973ba, 0x3da65230,
	0x3da3308c, 0x3da00ecf, 0x3d9cecf9, 0x3d99cb0a, 0x3d96a905, 0x3d9386e7,
	0x3d9064b4, 0x3d8d426a, 0x3d8a200a, 0x3d86fd94, 0x3d83db0a, 0x3d80b86c,
	0x3d7b2b74, 0x3d74e5e9, 0x3d6ea038, 0x3d685a62, 0x3d621469, 0x3d5bce4c,
	0x3d55880e, 0x3d4f41af, 0x3d48fb30, 0x3d42b491, 0x3d3c6dd5, 0x3d3626fc,
	0x3d2fe007, 0x3d2998f6, 0x3d2351cb, 0x3d1d0a88, 0x3d16c32c, 0x3d107bb8,
	0x3d0a342f, 0x3d03ec90, 0x3cfb49ba, 0x3ceeba2c, 0x3ce22a7a, 0x3cd59aa6,
	0x3cc90ab0, 0x3cbc7a9b, 0x3cafea69, 0x3ca35a1c, 0x3c96c9b6, 0x3c8a3938,
	0x3c7b514b, 0x3c622fff, 0x3c490e90, 0x3c2fed02, 0x3c16cb58, 0x3bfb5330,
	0x3bc90f88, 0x3b96cbc1, 0x3b490fc6, 0x3ac90fd5, 0x00000000, 0xbac90fd5,
	0xbb490fc6, 0xbb96cbc1, 0xbbc90f88, 0xbbfb5330, 0xbc16cb58, 0xbc2fed02,
	0xbc490e90, 0xbc622fff, 0xbc7b514b, 0xbc8a3938, 0xbc96c9b6, 0xbca35a1c,
	0xbcafea69, 0xbcbc7a9b, 0xbcc90ab0, 0xbcd59aa6, 0xbce22a7a, 0xbceeba2c,
	0xbcfb49ba, 0xbd03ec90, 0xbd0a342f, 0xbd107bb8, 0xbd16c32c, 0xbd1d0a88,
	0xbd2351cb, 0xbd2998f6, 0xbd2fe007, 0xbd3626fc, 0xbd3c6dd5, 0xbd42b491,
	0xbd48fb30, 0xbd4f41af, 0xbd55880e, 0xbd5bce4c, 0xbd621469, 0xbd685a62,
	0xbd6ea038, 0xbd74e5e9, 0xbd7b2b74, 0xbd80b86c, 0xbd83db0a, 0xbd86fd94,
	0xbd8a200a, 0xbd8d426a, 0xbd9064b4, 0xbd9386e7, 0xbd96a905, 0xbd99cb0a,
	0xbd9cecf9, 0xbda00ecf, 0xbda3308c, 0xbda65230, 0xbda973ba, 0xbdac952b,
	0xbdafb680, 0xbdb2d7bb, 0xbdb5f8da, 0xbdb919dd, 0xbdbc3ac3, 0xbdbf5b8d,
	0xbdc27c39, 0xbdc59cc6, 0xbdc8bd36, 0xbdcbdd86, 0xbdcefdb7, 0xbdd21dc8,
	0xbdd53db9, 0xbdd85d89, 0xbddb7d37, 0xbdde9cc4, 0xbde1bc2e, 0xbde4db76,
	0xbde7fa9a, 0xbdeb199a, 0xbdee3876, 0xbdf1572e, 0xbdf475c0, 0xbdf7942c,
	0xbdfab273, 0xbdfdd092, 0xbe007745, 0xbe02062e, 0xbe039502, 0xbe0523c2,
	0xbe06b26e, 0xbe084105, 0xbe09cf86, 0xbe0b5df3, 0xbe0cec4a, 0xbe0e7a8b,
	0xbe1008b7, 0xbe1196cc, 0xbe1324ca, 0xbe14b2b2, 0xbe164083, 0xbe17ce3d,
	0xbe195be0, 0xbe1ae96b, 0xbe1c76de, 0xbe1e0438, 0xbe1f917b, 0xbe211ea5,
	0xbe22abb6, 0xbe2438ad, 0xbe25c58c, 0xbe275251, 0xbe28defc, 0xbe2a6b8d,
	0xbe2bf804, 0xbe2d8461, 0xbe2f10a2, 0xbe309cc9, 0xbe3228d4, 0xbe33b4c4,
	0xbe354098, 0xbe36cc50, 0xbe3857ec, 0xbe39e36c, 0xbe3b6ecf, 0xbe3cfa15,
	0xbe3e853e, 0xbe401049, 0xbe419b37, 0xbe432607, 0xbe44b0b9, 0xbe463b4d,
	0xbe47c5c2, 0xbe495018, 0xbe4ada4f, 0xbe4c6467, 0xbe4dee60, 0xbe4f7838,
	0xbe5101f1, 0xbe528b89, 0xbe541501, 0xbe559e58, 0xbe57278f, 0xbe58b0a4,
	0xbe5a3997, 0xbe5bc26a, 0xbe5d4b1a, 0xbe5ed3a8, 0xbe605c13, 0xbe61e45c,
	0xbe636c83, 0xbe64f486, 0xbe667c66, 0xbe680422, 0xbe698bba, 0xbe6b132f,
	0xbe6c9a7f, 0xbe6e21ab, 0xbe6fa8b2, 0xbe712f94, 0xbe72b651, 0xbe743ce8,
	0xbe75c35a, 0xbe7749a6, 0xbe78cfcc, 0xbe7a55cb, 0xbe7bdba4, 0xbe7d6156,
	0xbe7ee6e1, 0xbe803622, 0xbe80f8c0, 0xbe81bb4a, 0xbe827dc0, 0xbe834022,
	0xbe840270, 0xbe84c4aa, 0xbe8586ce, 0xbe8648df, 0xbe870ada, 0xbe87ccc1,
	0xbe888e93, 0xbe895050, 0xbe8a11f7, 0xbe8ad38a, 0xbe8b9507, 0xbe8c566e,
	0xbe8d17c0, 0xbe8dd8fc, 0xbe8e9a22, 0xbe8f5b32, 0xbe901c2c, 0xbe90dd10,
	0xbe919ddd, 0xbe925e94, 0xbe931f35, 0xbe93dfbf, 0xbe94a031, 0xbe95608d,
	0xbe9620d2, 0xbe96e100, 0xbe97a117, 0xbe986116, 0xbe9920fe, 0xbe99e0ce,
	0xbe9aa086, 0xbe9b6027, 0xbe9c1faf, 0xbe9cdf20, 0xbe9d9e78, 0xbe9e5db8,
	0xbe9f1cdf, 0xbe9fdbee, 0xbea09ae5, 0xbea159c2, 0xbea21887, 0xbea2d733,
	0xbea395c5, 0xbea4543f, 0xbea5129f, 0xbea5d0e5, 0xbea68f12, 0xbea74d25,
	0xbea80b1f, 0xbea8c8fe, 0xbea986c4, 0xbeaa446f, 0xbeab0201, 0xbeabbf77,
	0xbeac7cd4, 0xbead3a15, 0xbeadf73c, 0xbeaeb449, 0xbeaf713a, 0xbeb02e10,
	0xbeb0eacb, 0xbeb1a76b, 0xbeb263ef, 0xbeb32058, 0xbeb3dca5, 0xbeb498d6,
	0xbeb554ec, 0xbeb610e6, 0xbeb6ccc3, 0xbeb78884, 0xbeb8442a, 0xbeb8ffb2,
	0xbeb9bb1e, 0xbeba766e, 0xbebb31a0, 0xbebbecb6, 0xbebca7af, 0xbebd628b,
	0xbebe1d4a, 0xbebed7eb, 0xbebf926f, 0xbec04cd5, 0xbec1071e, 0xbec1c148,
	0xbec27b55, 0xbec33544, 0xbec3ef15, 0xbec4a8c8, 0xbec5625c, 0xbec61bd2,
	0xbec6d529, 0xbec78e62, 0xbec8477c, 0xbec90077, 0xbec9b953, 0xbeca7210,
	0xbecb2aae, 0xbecbe32c, 0xbecc9b8b, 0xbecd53ca, 0xbece0bea, 0xbecec3ea,
	0xbecf7bca, 0xbed0338a, 0xbed0eb2a, 0xbed1a2aa, 0xbed25a09, 0xbed31148,
	0xbed3c867, 0xbed47f64, 0xbed53641, 0xbed5ecfd, 0xbed6a399, 0xbed75a13,
	0xbed8106b, 0xbed8c6a3, 0xbed97cb9, 0xbeda32ad, 0xbedae880, 0xbedb9e31,
	0xbedc53c1, 0xbedd092e, 0xbeddbe79, 0xbede73a2, 0xbedf28a9, 0xbedfdd8d,
	0xbee0924f, 0xbee146ee, 0xbee1fb6a, 0xbee2afc4, 0xbee363fa, 0xbee4180e,
	0xbee4cbfe, 0xbee57fcb, 0xbee63375, 0xbee6e6fb, 0xbee79a5d, 0xbee84d9c,
	0xbee900b7, 0xbee9b3ae, 0xbeea6681, 0xbeeb1930, 0xbeebcbbb, 0xbeec7e21,
	0xbeed3063, 0xbeede280, 0xbeee9479, 0xbeef464c, 0xbeeff7fb, 0xbef0a985,
	0xbef15aea, 0xbef20c29, 0xbef2bd43, 0xbef36e38, 0xbef41f07, 0xbef4cfb1,
	0xbef58035, 0xbef63093, 0xbef6e0cb, 0xbef790dc, 0xbef840c8, 0xbef8f08e,
	0xbef9a02d, 0xbefa4fa5, 0xbefafef7, 0xbefbae22, 0xbefc5d27, 0xbefd0c04,
	0xbefdbabb, 0xbefe694a, 0xbeff17b2, 0xbeffc5f3, 0xbf003a06, 0xbf0090ff,
	0xbf00e7e4, 0xbf013eb5, 0xbf019573, 0xbf01ec1c, 0xbf0242b1, 0xbf029932,
	0xbf02ef9f, 0xbf0345f8, 0xbf039c3d, 0xbf03f26d, 0xbf044889, 0xbf049e91,
	0xbf04f484, 0xbf054a62, 0xbf05a02c, 0xbf05f5e2, 0xbf064b82, 0xbf06a10e,
	0xbf06f686, 0xbf074be8, 0xbf07a136, 0xbf07f66f, 0xbf084b92, 0xbf08a0a1,
	0xbf08f59b, 0xbf094a7f, 0xbf099f4e, 0xbf09f409, 0xbf0a48ad, 0xbf0a9d3d,
	0xbf0af1b7, 0xbf0b461c, 0xbf0b9a6b, 0xbf0beea5, 0xbf0c42c9, 0xbf0c96d7,
	0xbf0cead0, 0xbf0d3eb3, 0xbf0d9281, 0xbf0de638, 0xbf0e39da, 0xbf0e8d65,
	0xbf0ee0db, 0xbf0f343b, 0xbf0f8784, 0xbf0fdab8, 0xbf102dd5, 0xbf1080dc,
	0xbf10d3cd, 0xbf1126a7, 0xbf11796b, 0xbf11cc19, 0xbf121eb0, 0xbf127130,
	0xbf12c39a, 0xbf1315ee, 0xbf13682a, 0xbf13ba50, 0xbf140c5f, 0xbf145e58,
	0xbf14b039, 0xbf150204, 0xbf1553b7, 0xbf15a554, 0xbf15f6d9, 0xbf164847,
	0xbf16999f, 0xbf16eade, 0xbf173c07, 0xbf178d18, 0xbf17de12, 0xbf182ef5,
	0xbf187fc0, 0xbf18d073, 0xbf19210f, 0xbf197194, 0xbf19c200, 0xbf1a1255,
	0xbf1a6293, 0xbf1ab2b8, 0xbf1b02c6, 0xbf1b52bb, 0xbf1ba299, 0xbf1bf25f,
	0xbf1c420c, 0xbf1c91a2, 0xbf1ce11f, 0xbf1d3084, 0xbf1d7fd1, 0xbf1dcf06,
	0xbf1e1e22, 0xbf1e6d26, 0xbf1ebc12, 0xbf1f0ae5, 0xbf1f599f, 0xbf1fa841,
	0xbf1ff6cb, 0xbf20453b, 0xbf209393, 0xbf20e1d2, 0xbf212ff9, 0xbf217e06,
	0xbf21cbfb, 0xbf2219d7, 0xbf226799, 0xbf22b543, 0xbf2302d3, 0xbf23504b,
	0xbf239da9, 0xbf23eaee, 0xbf24381a, 0xbf24852c, 0xbf24d225, 0xbf251f04,
	0xbf256bcb, 0xbf25b877, 0xbf26050a, 0xbf265184, 0xbf269de3, 0xbf26ea2a,
	0xbf273656, 0xbf278268, 0xbf27ce61, 0xbf281a40, 0xbf286605, 0xbf28b1b0,
	0xbf28fd41, 0xbf2948b8, 0xbf299415, 0xbf29df57, 0xbf2a2a80, 0xbf2a758e,
	0xbf2ac082, 0xbf2b0b5b, 0xbf2b561b, 0xbf2ba0bf, 0xbf2beb4a, 0xbf2c35b9,
	0xbf2c800f, 0xbf2cca49, 0xbf2d1469, 0xbf2d5e6f, 0xbf2da859, 0xbf2df229,
	0xbf2e3bde, 0xbf2e8578, 0xbf2ecef7, 0xbf2f185b, 0xbf2f61a5, 0xbf2faad3,
	0xbf2ff3e6, 0xbf303cde, 0xbf3085bb, 0xbf30ce7c, 0xbf311722, 0xbf315fad,
	0xbf31a81d, 0xbf31f071, 0xbf3238aa, 0xbf3280c7, 0xbf32c8c9, 0xbf3310af,
	0xbf33587a, 0xbf33a029, 0xbf33e7bc, 0xbf342f34, 0xbf34768f, 0xbf34bdcf,
	0xbf3504f3, 0xbf354bfb, 0xbf3592e7, 0xbf35d9b8, 0xbf36206c, 0xbf366704,
	0xbf36ad7f, 0xbf36f3df, 0xbf373a23, 0xbf37804a, 0xbf37c655, 0xbf380c43,
	0xbf385216, 0xbf3897cb, 0xbf38dd65, 0xbf3922e1, 0xbf396842, 0xbf39ad85,
	0xbf39f2ac, 0xbf3a37b7, 0xbf3a7ca4, 0xbf3ac175, 0xbf3b0629, 0xbf3b4ac1,
	0xbf3b8f3b, 0xbf3bd398, 0xbf3c17d9, 0xbf3c5bfc, 0xbf3ca003, 0xbf3ce3ec,
	0xbf3d27b8, 0xbf3d6b67, 0xbf3daef9, 0xbf3df26e, 0xbf3e35c5, 0xbf3e78ff,
	0xbf3ebc1b, 0xbf3eff1b, 0xbf3f41fc, 0xbf3f84c0, 0xbf3fc767, 0xbf4009f0,
	0xbf404c5c, 0xbf408ea9, 0xbf40d0da, 0xbf4112ec, 0xbf4154e1, 0xbf4196b7,
	0xbf41d870, 0xbf421a0b, 0xbf425b89, 0xbf429ce8, 0xbf42de29, 0xbf431f4c,
	0xbf436051, 0xbf43a138, 0xbf43e200, 0xbf4422ab, 0xbf446337, 0xbf44a3a5,
	0xbf44e3f5, 0xbf452426, 0xbf456439, 0xbf45a42d, 0xbf45e403, 0xbf4623bb,
	0xbf466354, 0xbf46a2ce, 0xbf46e22a, 0xbf472167, 0xbf476085, 0xbf479f84,
	0xbf47de65, 0xbf481d27, 0xbf485bca, 0xbf489a4e, 0xbf48d8b3, 0xbf4916fa,
	0xbf495521, 0xbf499329, 0xbf49d112, 0xbf4a0edc, 0xbf4a4c87, 0xbf4a8a13,
	0xbf4ac77f, 0xbf4b04cc, 0xbf4b41fa, 0xbf4b7f09, 0xbf4bbbf8, 0xbf4bf8c7,
	0xbf4c3578, 0xbf4c7208, 0xbf4cae79, 0xbf4ceacb, 0xbf4d26fd, 0xbf4d6310,
	0xbf4d9f02, 0xbf4ddad5, 0xbf4e1689, 0xbf4e521c, 0xbf4e8d90, 0xbf4ec8e4,
	0xbf4f0417, 0xbf4f3f2b, 0xbf4f7a1f, 0xbf4fb4f4, 0xbf4fefa8, 0xbf502a3b,
	0xbf5064af, 0xbf509f03, 0xbf50d937, 0xbf51134a, 0xbf514d3d, 0xbf518710,
	0xbf51c0c2, 0xbf51fa54, 0xbf5233c6, 0xbf526d18, 0xbf52a649, 0xbf52df59,
	0xbf531849, 0xbf535118, 0xbf5389c7, 0xbf53c255, 0xbf53fac3, 0xbf54330f,
	0xbf546b3b, 0xbf54a347, 0xbf54db31, 0xbf5512fb, 0xbf554aa4, 0xbf55822c,
	0xbf55b993, 0xbf55f0d9, 0xbf5627fe, 0xbf565f02, 0xbf5695e5, 0xbf56cca7,
	0xbf570348, 0xbf5739c7, 0xbf577026, 0xbf57a663, 0xbf57dc7f, 0xbf581279,
	0xbf584853, 0xbf587e0b, 0xbf58b3a1, 0xbf58e916, 0xbf591e6a, 0xbf59539c,
	0xbf5988ad, 0xbf59bd9c, 0xbf59f26a, 0xbf5a2716, 0xbf5a5ba0, 0xbf5a9009,
	0xbf5ac450, 0xbf5af875, 0xbf5b2c79, 0xbf5b605a, 0xbf5b941a, 0xbf5bc7b8,
	0xbf5bfb34, 0xbf5c2e8e, 0xbf5c61c7, 0xbf5c94dd, 0xbf5cc7d1, 0xbf5cfaa3,
	0xbf5d2d53, 0xbf5d5fe1, 0xbf5d924d, 0xbf5dc497, 0xbf5df6be, 0xbf5e28c3,
	0xbf5e5aa6, 0xbf5e8c67, 0xbf5ebe05, 0xbf5eef81, 0xbf5f20db, 0xbf5f5212,
	0xbf5f8327, 0xbf5fb419, 0xbf5fe4e9, 0xbf601596, 0xbf604621, 0xbf607689,
	0xbf60a6cf, 0xbf60d6f2, 0xbf6106f2, 0xbf6136d0, 0xbf61668a, 0xbf619622,
	0xbf61c598, 0xbf61f4ea, 0xbf62241a, 0xbf625326, 0xbf628210, 0xbf62b0d7,
	0xbf62df7b, 0xbf630dfc, 0xbf633c5a, 0xbf636a95, 0xbf6398ac, 0xbf63c6a1,
	0xbf63f473, 0xbf642221, 0xbf644fac, 0xbf647d14, 0xbf64aa59, 0xbf64d77b,
	0xbf650479, 0xbf653154, 0xbf655e0b, 0xbf658aa0, 0xbf65b710, 0xbf65e35e,
	0xbf660f88, 0xbf663b8e, 0xbf666771, 0xbf669330, 0xbf66becc, 0xbf66ea45,
	0xbf671599, 0xbf6740ca, 0xbf676bd8, 0xbf6796c1, 0xbf67c187, 0xbf67ec29,
	0xbf6816a8, 0xbf684103, 0xbf686b39, 0xbf68954c, 0xbf68bf3c, 0xbf68e907,
	0xbf6912ae, 0xbf693c32, 0xbf696591, 0xbf698ecc, 0xbf69b7e4, 0xbf69e0d7,
	0xbf6a09a7, 0xbf6a3252, 0xbf6a5ad9, 0xbf6a833c, 0xbf6aab7b, 0xbf6ad395,
	0xbf6afb8c, 0xbf6b235e, 0xbf6b4b0c, 0xbf6b7295, 0xbf6b99fb, 0xbf6bc13b,
	0xbf6be858, 0xbf6c0f50, 0xbf6c3624, 0xbf6c5cd4, 0xbf6c835e, 0xbf6ca9c5,
	0xbf6cd007, 0xbf6cf624, 0xbf6d1c1d, 0xbf6d41f2, 0xbf6d67a1, 0xbf6d8d2d,
	0xbf6db293, 0xbf6dd7d5, 0xbf6dfcf2, 0xbf6e21eb, 0xbf6e46be, 0xbf6e6b6d,
	0xbf6e8ff8, 0xbf6eb45d, 0xbf6ed89e, 0xbf6efcba, 0xbf6f20b0, 0xbf6f4483,
	0xbf6f6830, 0xbf6f8bb8, 0xbf6faf1b, 0xbf6fd25a, 0xbf6ff573, 0xbf701867,
	0xbf703b37, 0xbf705de1, 0xbf708066, 0xbf70a2c6, 0xbf70c501, 0xbf70e717,
	0xbf710908, 0xbf712ad4, 0xbf714c7a, 0xbf716dfb, 0xbf718f57, 0xbf71b08e,
	0xbf71d19f, 0xbf71f28c, 0xbf721352, 0xbf7233f4, 0xbf725470, 0xbf7274c7,
	0xbf7294f8, 0xbf72b504, 0xbf72d4eb, 0xbf72f4ac, 0xbf731447, 0xbf7333be,
	0xbf73530e, 0xbf737239, 0xbf73913f, 0xbf73b01f, 0xbf73ced9, 0xbf73ed6e,
	0xbf740bdd, 0xbf742a27, 0xbf74484b, 0xbf746649, 0xbf748422, 0xbf74a1d5,
	0xbf74bf62, 0xbf74dcc9, 0xbf74fa0b, 0xbf751727, 0xbf75341d, 0xbf7550ed,
	0xbf756d97, 0xbf758a1c, 0xbf75a67b, 0xbf75c2b3, 0xbf75dec6, 0xbf75fab3,
	0xbf76167a, 0xbf76321b, 0xbf764d97, 0xbf7668ec, 0xbf76841b, 0xbf769f24,
	0xbf76ba07, 0xbf76d4c4, 0xbf76ef5b, 0xbf7709cc, 0xbf772417, 0xbf773e3c,
	0xbf77583a, 0xbf777213, 0xbf778bc5, 0xbf77a551, 0xbf77beb7, 0xbf77d7f7,
	0xbf77f110, 0xbf780a04, 0xbf7822d1, 0xbf783b77, 0xbf7853f8, 0xbf786c52,
	0xbf788486, 0xbf789c93, 0xbf78b47b, 0xbf78cc3b, 0xbf78e3d6, 0xbf78fb4a,
	0xbf791298, 0xbf7929bf, 0xbf7940c0, 0xbf79579a, 0xbf796e4e, 0xbf7984dc,
	0xbf799b43, 0xbf79b183, 0xbf79c79d, 0xbf79dd91, 0xbf79f35e, 0xbf7a0904,
	0xbf7a1e84, 0xbf7a33dd, 0xbf7a4910, 0xbf7a5e1c, 0xbf7a7302, 0xbf7a87c1,
	0xbf7a9c59, 0xbf7ab0cb, 0xbf7ac516, 0xbf7ad93a, 0xbf7aed37, 0xbf7b010e,
	0xbf7b14be, 0xbf7b2848, 0xbf7b3bab, 0xbf7b4ee7, 0xbf7b61fc, 0xbf7b74ea,
	0xbf7b87b2, 0xbf7b9a53, 0xbf7baccd, 0xbf7bbf20, 0xbf7bd14d, 0xbf7be353,
	0xbf7bf531, 0xbf7c06e9, 0xbf7c187a, 0xbf7c29e5, 0xbf7c3b28, 0xbf7c4c44,
	0xbf7c5d3a, 0xbf7c6e08, 0xbf7c7eb0, 0xbf7c8f31, 0xbf7c9f8a, 0xbf7cafbd,
	0xbf7cbfc9, 0xbf7ccfae, 0xbf7cdf6c, 0xbf7cef03, 0xbf7cfe73, 0xbf7d0dbc,
	0xbf7d1cdd, 0xbf7d2bd8, 0xbf7d3aac, 0xbf7d4959, 0xbf7d57de, 0xbf7d663d,
	0xbf7d7474, 0xbf7d8285, 0xbf7d906e, 0xbf7d9e30, 0xbf7dabcc, 0xbf7db940,
	0xbf7dc68c, 0xbf7dd3b2, 0xbf7de0b1, 0xbf7ded88, 0xbf7dfa38, 0xbf7e06c2,
	0xbf7e1324, 0xbf7e1f5e, 0xbf7e2b72, 0xbf7e375e, 0xbf7e4323, 0xbf7e4ec1,
	0xbf7e5a38, 0xbf7e6588, 0xbf7e70b0, 0xbf7e7bb1, 0xbf7e868b, 0xbf7e913d,
	0xbf7e9bc9, 0xbf7ea62d, 0xbf7eb069, 0xbf7eba7f, 0xbf7ec46d, 0xbf7ece34,
	0xbf7ed7d4, 0xbf7ee14c, 0xbf7eea9d, 0xbf7ef3c7, 0xbf7efcc9, 0xbf7f05a4,
	0xbf7f0e58, 0xbf7f16e4, 0xbf7f1f49, 0xbf7f2787, 0xbf7f2f9d, 0xbf7f378c,
	0xbf7f3f54, 0xbf7f46f4, 0xbf7f4e6d, 0xbf7f55bf, 0xbf7f5ce9, 0xbf7f63ec,
	0xbf7f6ac7, 0xbf7f717b, 0xbf7f7808, 0xbf7f7e6d, 0xbf7f84ab, 0xbf7f8ac2,
	0xbf7f90b1, 0xbf7f9678, 0xbf7f9c18, 0xbf7fa191, 0xbf7fa6e3, 0xbf7fac0d,
	0xbf7fb10f, 0xbf7fb5ea, 0xbf7fba9e, 0xbf7fbf2a, 0xbf7fc38f, 0xbf7fc7cc,
	0xbf7fcbe2, 0xbf7fcfd1, 0xbf7fd397, 0xbf7fd737, 0xbf7fdaaf, 0xbf7fde00,
	0xbf7fe129, 0xbf7fe42b, 0xbf7fe705, 0xbf7fe9b8, 0xbf7fec43, 0xbf7feea7,
	0xbf7ff0e3, 0xbf7ff2f8, 0xbf7ff4e6, 0xbf7ff6ac, 0xbf7ff84a, 0xbf7ff9c1,
	0xbf7ffb11, 0xbf7ffc39, 0xbf7ffd39, 0xbf7ffe13, 0xbf7ffec4, 0xbf7fff4e,
	0xbf7fffb1, 0xbf7fffec, 0xbf800000, 0xbf7fffec, 0xbf7fffb1, 0xbf7fff4e,
	0xbf7ffec4, 0xbf7ffe13, 0xbf7ffd39, 0xbf7ffc39, 0xbf7ffb11, 0xbf7ff9c1,
	0xbf7ff84a, 0xbf7ff6ac, 0xbf7ff4e6, 0xbf7ff2f8, 0xbf7ff0e3, 0xbf7feea7,
	0xbf7fec43, 0xbf7fe9b8, 0xbf7fe705, 0xbf7fe42b, 0xbf7fe129, 0xbf7fde00,
	0xbf7fdaaf, 0xbf7fd737, 0xbf7fd397, 0xbf7fcfd1, 0xbf7fcbe2, 0xbf7fc7cc,
	0xbf7fc38f, 0xbf7fbf2a, 0xbf7fba9e, 0xbf7fb5ea, 0xbf7fb10f, 0xbf7fac0d,
	0xbf7fa6e3, 0xbf7fa191, 0xbf7f9c18, 0xbf7f9678, 0xbf7f90b1, 0xbf7f8ac2,
	0xbf7f84ab, 0xbf7f7e6d, 0xbf7f7808, 0xbf7f717b, 0xbf7f6ac7, 0xbf7f63ec,
	0xbf7f5ce9, 0xbf7f55bf, 0xbf7f4e6d, 0xbf7f46f4, 0xbf7f3f54, 0xbf7f378c,
	0xbf7f2f9d, 0xbf7f2787, 0xbf7f1f49, 0xbf7f16e4, 0xbf7f0e58, 0xbf7f05a4,
	0xbf7efcc9, 0xbf7ef3c7, 0xbf7eea9d, 0xbf7ee14c, 0xbf7ed7d4, 0xbf7ece34,
	0xbf7ec46d, 0xbf7eba7f, 0xbf7eb069, 0xbf7ea62d, 0xbf7e9bc9, 0xbf7e913d,
	0xbf7e868b, 0xbf7e7bb1, 0xbf7e70b0, 0xbf7e6588, 0xbf7e5a38, 0xbf7e4ec1,
	0xbf7e4323, 0xbf7e375e, 0xbf7e2b72, 0xbf7e1f5e, 0xbf7e1324, 0xbf7e06c2,
	0xbf7dfa38, 0xbf7ded88, 0xbf7de0b1, 0xbf7dd3b2, 0xbf7dc68c, 0xbf7db940,
	0xbf7dabcc, 0xbf7d9e30, 0xbf7d906e, 0xbf7d8285, 0xbf7d7474, 0xbf7d663d,
	0xbf7d57de, 0xbf7d4959, 0xbf7d3aac, 0xbf7d2bd8, 0xbf7d1cdd, 0xbf7d0dbc,
	0xbf7cfe73, 0xbf7cef03, 0xbf7cdf6c, 0xbf7ccfae, 0xbf7cbfc9, 0xbf7cafbd,
	0xbf7c9f8a, 0xbf7c8f31, 0xbf7c7eb0, 0xbf7c6e08, 0xbf7c5d3a, 0xbf7c4c44,
	0xbf7c3b28, 0xbf7c29e5, 0xbf7c187a, 0xbf7c06e9, 0xbf7bf531, 0xbf7be353,
	0xbf7bd14d, 0xbf7bbf20, 0xbf7baccd, 0xbf7b9a53, 0xbf7b87b2, 0xbf7b74ea,
	0xbf7b61fc, 0xbf7b4ee7, 0xbf7b3bab, 0xbf7b2848, 0xbf7b14be, 0xbf7b010e,
	0xbf7aed37, 0xbf7ad93a, 0xbf7ac516, 0xbf7ab0cb, 0xbf7a9c59, 0xbf7a87c1,
	0xbf7a7302, 0xbf7a5e1c, 0xbf7a4910, 0xbf7a33dd, 0xbf7a1e84, 0xbf7a0904,
	0xbf79f35e, 0xbf79dd91, 0xbf79c79d, 0xbf79b183, 0xbf799b43, 0xbf7984dc,
	0xbf796e4e, 0xbf79579a, 0xbf7940c0, 0xbf7929bf, 0xbf791298, 0xbf78fb4a,
	0xbf78e3d6, 0xbf78cc3b, 0xbf78b47b, 0xbf789c93, 0xbf788486, 0xbf786c52,
	0xbf7853f8, 0xbf783b77, 0xbf7822d1, 0xbf780a04, 0xbf77f110, 0xbf77d7f7,
	0xbf77beb7, 0xbf77a551, 0xbf778bc5, 0xbf777213, 0xbf77583a, 0xbf773e3c,
	0xbf772417, 0xbf7709cc, 0xbf76ef5b, 0xbf76d4c4, 0xbf76ba07, 0xbf769f24,
	0xbf76841b, 0xbf7668ec, 0xbf764d97, 0xbf76321b, 0xbf76167a, 0xbf75fab3,
	0xbf75dec6, 0xbf75c2b3, 0xbf75a67b, 0xbf758a1c, 0xbf756d97, 0xbf7550ed,
	0xbf75341d, 0xbf751727, 0xbf74fa0b, 0xbf74dcc9, 0xbf74bf62, 0xbf74a1d5,
	0xbf748422, 0xbf746649, 0xbf74484b, 0xbf742a27, 0xbf740bdd, 0xbf73ed6e,
	0xbf73ced9, 0xbf73b01f, 0xbf73913f, 0xbf737239, 0xbf73530e, 0xbf7333be,
	0xbf731447, 0xbf72f4ac, 0xbf72d4eb, 0xbf72b504, 0xbf7294f8, 0xbf7274c7,
	0xbf725470, 0xbf7233f4, 0xbf721352, 0xbf71f28c, 0xbf71d19f, 0xbf71b08e,
	0xbf718f57, 0xbf716dfb, 0xbf714c7a, 0xbf712ad4, 0xbf710908, 0xbf70e717,
	0xbf70c501, 0xbf70a2c6, 0xbf708066, 0xbf705de1, 0xbf703b37, 0xbf701867,
	0xbf6ff573, 0xbf6fd25a, 0xbf6faf1b, 0xbf6f8bb8, 0xbf6f6830, 0xbf6f4483,
	0xbf6f20b0, 0xbf6efcba, 0xbf6ed89e, 0xbf6eb45d, 0xbf6e8ff8, 0xbf6e6b6d,
	0xbf6e46be, 0xbf6e21eb, 0xbf6dfcf2, 0xbf6dd7d5, 0xbf6db293, 0xbf6d8d2d,
	0xbf6d67a1, 0xbf6d41f2, 0xbf6d1c1d, 0xbf6cf624, 0xbf6cd007, 0xbf6ca9c5,
	0xbf6c835e, 0xbf6c5cd4, 0xbf6c3624, 0xbf6c0f50, 0xbf6be858, 0xbf6bc13b,
	0xbf6b99fb, 0xbf6b7295, 0xbf6b4b0c, 0xbf6b235e, 0xbf6afb8c, 0xbf6ad395,
	0xbf6aab7b, 0xbf6a833c, 0xbf6a5ad9, 0xbf6a3252, 0xbf6a09a7, 0xbf69e0d7,
	0xbf69b7e4, 0xbf698ecc, 0xbf696591, 0xbf693c32, 0xbf6912ae, 0xbf68e907,
	0xbf68bf3c, 0xbf68954c, 0xbf686b39, 0xbf684103, 0xbf6816a8, 0xbf67ec29,
	0xbf67c187, 0xbf6796c1, 0xbf676bd8, 0xbf6740ca, 0xbf671599, 0xbf66ea45,
	0xbf66becc, 0xbf669330, 0xbf666771, 0xbf663b8e, 0xbf660f88, 0xbf65e35e,
	0xbf65b710, 0xbf658aa0, 0xbf655e0b, 0xbf653154, 0xbf650479, 0xbf64d77b,
	0xbf64aa59, 0xbf647d14, 0xbf644fac, 0xbf642221, 0xbf63f473, 0xbf63c6a1,
	0xbf6398ac, 0xbf636a95, 0xbf633c5a, 0xbf630dfc, 0xbf62df7b, 0xbf62b0d7,
	0xbf628210, 0xbf625326, 0xbf62241a, 0xbf61f4ea, 0xbf61c598, 0xbf619622,
	0xbf61668a, 0xbf6136d0, 0xbf6106f2, 0xbf60d6f2, 0xbf60a6cf, 0xbf607689,
	0xbf604621, 0xbf601596, 0xbf5fe4e9, 0xbf5fb419, 0xbf5f8327, 0xbf5f5212,
	0xbf5f20db, 0xbf5eef81, 0xbf5ebe05, 0xbf5e8c67, 0xbf5e5aa6, 0xbf5e28c3,
	0xbf5df6be, 0xbf5dc497, 0xbf5d924d, 0xbf5d5fe1, 0xbf5d2d53, 0xbf5cfaa3,
	0xbf5cc7d1, 0xbf5c94dd, 0xbf5c61c7, 0xbf5c2e8e, 0xbf5bfb34, 0xbf5bc7b8,
	0xbf5b941a, 0xbf5b605a, 0xbf5b2c79, 0xbf5af875, 0xbf5ac450, 0xbf5a9009,
	0xbf5a5ba0, 0xbf5a2716, 0xbf59f26a, 0xbf59bd9c, 0xbf5988ad, 0xbf59539c,
	0xbf591e6a, 0xbf58e916, 0xbf58b3a1, 0xbf587e0b, 0xbf584853, 0xbf581279,
	0xbf57dc7f, 0xbf57a663, 0xbf577026, 0xbf5739c7, 0xbf570348, 0xbf56cca7,
	0xbf5695e5, 0xbf565f02, 0xbf5627fe, 0xbf55f0d9, 0xbf55b993, 0xbf55822c,
	0xbf554aa4, 0xbf5512fb, 0xbf54db31, 0xbf54a347, 0xbf546b3b, 0xbf54330f,
	0xbf53fac3, 0xbf53c255, 0xbf5389c7, 0xbf535118, 0xbf531849, 0xbf52df59,
	0xbf52a649, 0xbf526d18, 0xbf5233c6, 0xbf51fa54, 0xbf51c0c2, 0xbf518710,
	0xbf514d3d, 0xbf51134a, 0xbf50d937, 0xbf509f03, 0xbf5064af, 0xbf502a3b,
	0xbf4fefa8, 0xbf4fb4f4, 0xbf4f7a1f, 0xbf4f3f2b, 0xbf4f0417, 0xbf4ec8e4,
	0xbf4e8d90, 0xbf4e521c, 0xbf4e1689, 0xbf4ddad5, 0xbf4d9f02, 0xbf4d6310,
	0xbf4d26fd, 0xbf4ceacb, 0xbf4cae79, 0xbf4c7208, 0xbf4c3578, 0xbf4bf8c7,
	0xbf4bbbf8, 0xbf4b7f09, 0xbf4b41fa, 0xbf4b04cc, 0xbf4ac77f, 0xbf4a8a13,
	0xbf4a4c87, 0xbf4a0edc, 0xbf49d112, 0xbf499329, 0xbf495521, 0xbf4916fa,
	0xbf48d8b3, 0xbf489a4e, 0xbf485bca, 0xbf481d27, 0xbf47de65, 0xbf479f84,
	0xbf476085, 0xbf472167, 0xbf46e22a, 0xbf46a2ce, 0xbf466354, 0xbf4623bb,
	0xbf45e403, 0xbf45a42d, 0xbf456439, 0xbf452426, 0xbf44e3f5, 0xbf44a3a5,
	0xbf446337, 0xbf4422ab, 0xbf43e200, 0xbf43a138, 0xbf436051, 0xbf431f4c,
	0xbf42de29, 0xbf429ce8, 0xbf425b89, 0xbf421a0b, 0xbf41d870, 0xbf4196b7,
	0xbf4154e1, 0xbf4112ec, 0xbf40d0da, 0xbf408ea9, 0xbf404c5c, 0xbf4009f0,
	0xbf3fc767, 0xbf3f84c0, 0xbf3f41fc, 0xbf3eff1b, 0xbf3ebc1b, 0xbf3e78ff,
	0xbf3e35c5, 0xbf3df26e, 0xbf3daef9, 0xbf3d6b67, 0xbf3d27b8, 0xbf3ce3ec,
	0xbf3ca003, 0xbf3c5bfc, 0xbf3c17d9, 0xbf3bd398, 0xbf3b8f3b, 0xbf3b4ac1,
	0xbf3b0629, 0xbf3ac175, 0xbf3a7ca4, 0xbf3a37b7, 0xbf39f2ac, 0xbf39ad85,
	0xbf396842, 0xbf3922e1, 0xbf38dd65, 0xbf3897cb, 0xbf385216, 0xbf380c43,
	0xbf37c655, 0xbf37804a, 0xbf373a23, 0xbf36f3df, 0xbf36ad7f, 0xbf366704,
	0xbf36206c, 0xbf35d9b8, 0xbf3592e7, 0xbf354bfb, 0xbf3504f3, 0xbf34bdcf,
	0xbf34768f, 0xbf342f34, 0xbf33e7bc, 0xbf33a029, 0xbf33587a, 0xbf3310af,
	0xbf32c8c9, 0xbf3280c7, 0xbf3238aa, 0xbf31f071, 0xbf31a81d, 0xbf315fad,
	0xbf311722, 0xbf30ce7c, 0xbf3085bb, 0xbf303cde, 0xbf2ff3e6, 0xbf2faad3,
	0xbf2f61a5, 0xbf2f185b, 0xbf2ecef7, 0xbf2e8578, 0xbf2e3bde, 0xbf2df229,
	0xbf2da859, 0xbf2d5e6f, 0xbf2d1469, 0xbf2cca49, 0xbf2c800f, 0xbf2c35b9,
	0xbf2beb4a, 0xbf2ba0bf, 0xbf2b561b, 0xbf2b0b5b, 0xbf2ac082, 0xbf2a758e,
	0xbf2a2a80, 0xbf29df57, 0xbf299415, 0xbf2948b8, 0xbf28fd41, 0xbf28b1b0,
	0xbf286605, 0xbf281a40, 0xbf27ce61, 0xbf278268, 0xbf273656, 0xbf26ea2a,
	0xbf269de3, 0xbf265184, 0xbf26050a, 0xbf25b877, 0xbf256bcb, 0xbf251f04,
	0xbf24d225, 0xbf24852c, 0xbf24381a, 0xbf23eaee, 0xbf239da9, 0xbf23504b,
	0xbf2302d3, 0xbf22b543, 0xbf226799, 0xbf2219d7, 0xbf21cbfb, 0xbf217e06,
	0xbf212ff9, 0xbf20e1d2, 0xbf209393, 0xbf20453b, 0xbf1ff6cb, 0xbf1fa841,
	0xbf1f599f, 0xbf1f0ae5, 0xbf1ebc12, 0xbf1e6d26, 0xbf1e1e22, 0xbf1dcf06,
	0xbf1d7fd1, 0xbf1d3084, 0xbf1ce11f, 0xbf1c91a2, 0xbf1c420c, 0xbf1bf25f,
	0xbf1ba299, 0xbf1b52bb, 0xbf1b02c6, 0xbf1ab2b8, 0xbf1a6293, 0xbf1a1255,
	0xbf19c200, 0xbf197194, 0xbf19210f, 0xbf18d073, 0xbf187fc0, 0xbf182ef5,
	0xbf17de12, 0xbf178d18, 0xbf173c07, 0xbf16eade, 0xbf16999f, 0xbf164847,
	0xbf15f6d9, 0xbf15a554, 0xbf1553b7, 0xbf150204, 0xbf14b039, 0xbf145e58,
	0xbf140c5f, 0xbf13ba50, 0xbf13682a, 0xbf1315ee, 0xbf12c39a, 0xbf127130,
	0xbf121eb0, 0xbf11cc19, 0xbf11796b, 0xbf1126a7, 0xbf10d3cd, 0xbf1080dc,
	0xbf102dd5, 0xbf0fdab8, 0xbf0f8784, 0xbf0f343b, 0xbf0ee0db, 0xbf0e8d65,
	0xbf0e39da, 0xbf0de638, 0xbf0d9281, 0xbf0d3eb3, 0xbf0cead0, 0xbf0c96d7,
	0xbf0c42c9, 0xbf0beea5, 0xbf0b9a6b, 0xbf0b461c, 0xbf0af1b7, 0xbf0a9d3d,
	0xbf0a48ad, 0xbf09f409, 0xbf099f4e, 0xbf094a7f, 0xbf08f59b, 0xbf08a0a1,
	0xbf084b92, 0xbf07f66f, 0xbf07a136, 0xbf074be8, 0xbf06f686, 0xbf06a10e,
	0xbf064b82, 0xbf05f5e2, 0xbf05a02c, 0xbf054a62, 0xbf04f484, 0xbf049e91,
	0xbf044889, 0xbf03f26d, 0xbf039c3d, 0xbf0345f8, 0xbf02ef9f, 0xbf029932,
	0xbf0242b1, 0xbf01ec1c, 0xbf019573, 0xbf013eb5, 0xbf00e7e4, 0xbf0090ff,
	0xbf003a06, 0xbeffc5f3, 0xbeff17b2, 0xbefe694a, 0xbefdbabb, 0xbefd0c04,
	0xbefc5d27, 0xbefbae22, 0xbefafef7, 0xbefa4fa5, 0xbef9a02d, 0xbef8f08e,
	0xbef840c8, 0xbef790dc, 0xbef6e0cb, 0xbef63093, 0xbef58035, 0xbef4cfb1,
	0xbef41f07, 0xbef36e38, 0xbef2bd43, 0xbef20c29, 0xbef15aea, 0xbef0a985,
	0xbeeff7fb, 0xbeef464c, 0xbeee9479, 0xbeede280, 0xbeed3063, 0xbeec7e21,
	0xbeebcbbb, 0xbeeb1930, 0xbeea6681, 0xbee9b3ae, 0xbee900b7, 0xbee84d9c,
	0xbee79a5d, 0xbee6e6fb, 0xbee63375, 0xbee57fcb, 0xbee4cbfe, 0xbee4180e,
	0xbee363fa, 0xbee2afc4, 0xbee1fb6a, 0xbee146ee, 0xbee0924f, 0xbedfdd8d,
	0xbedf28a9, 0xbede73a2, 0xbeddbe79, 0xbedd092e, 0xbedc53c1, 0xbedb9e31,
	0xbedae880, 0xbeda32ad, 0xbed97cb9, 0xbed8c6a3, 0xbed8106b, 0xbed75a13,
	0xbed6a399, 0xbed5ecfd, 0xbed53641, 0xbed47f64, 0xbed3c867, 0xbed31148,
	0xbed25a09, 0xbed1a2aa, 0xbed0eb2a, 0xbed0338a, 0xbecf7bca, 0xbecec3ea,
	0xbece0bea, 0xbecd53ca, 0xbecc9b8b, 0xbecbe32c, 0xbecb2aae, 0xbeca7210,
	0xbec9b953, 0xbec90077, 0xbec8477c, 0xbec78e62, 0xbec6d529, 0xbec61bd2,
	0xbec5625c, 0xbec4a8c8, 0xbec3ef15, 0xbec33544, 0xbec27b55, 0xbec1c148,
	0xbec1071e, 0xbec04cd5, 0xbebf926f, 0xbebed7eb, 0xbebe1d4a, 0xbebd628b,
	0xbebca7af, 0xbebbecb6, 0xbebb31a0, 0xbeba766e, 0xbeb9bb1e, 0xbeb8ffb2,
	0xbeb8442a, 0xbeb78884, 0xbeb6ccc3, 0xbeb610e6, 0xbeb554ec, 0xbeb498d6,
	0xbeb3dca5, 0xbeb32058, 0xbeb263ef, 0xbeb1a76b, 0xbeb0eacb, 0xbeb02e10,
	0xbeaf713a, 0xbeaeb449, 0xbeadf73c, 0xbead3a15, 0xbeac7cd4, 0xbeabbf77,
	0xbeab0201, 0xbeaa446f, 0xbea986c4, 0xbea8c8fe, 0xbea80b1f, 0xbea74d25,
	0xbea68f12, 0xbea5d0e5, 0xbea5129f, 0xbea4543f, 0xbea395c5, 0xbea2d733,
	0xbea21887, 0xbea159c2, 0xbea09ae5, 0xbe9fdbee, 0xbe9f1cdf, 0xbe9e5db8,
	0xbe9d9e78, 0xbe9cdf20, 0xbe9c1faf, 0xbe9b6027, 0xbe9aa086, 0xbe99e0ce,
	0xbe9920fe, 0xbe986116, 0xbe97a117, 0xbe96e100, 0xbe9620d2, 0xbe95608d,
	0xbe94a031, 0xbe93dfbf, 0xbe931f35, 0xbe925e94, 0xbe919ddd, 0xbe90dd10,
	0xbe901c2c, 0xbe8f5b32, 0xbe8e9a22, 0xbe8dd8fc, 0xbe8d17c0, 0xbe8c566e,
	0xbe8b9507, 0xbe8ad38a, 0xbe8a11f7, 0xbe895050, 0xbe888e93, 0xbe87ccc1,
	0xbe870ada, 0xbe8648df, 0xbe8586ce, 0xbe84c4aa, 0xbe840270, 0xbe834022,
	0xbe827dc0, 0xbe81bb4a, 0xbe80f8c0, 0xbe803622, 0xbe7ee6e1, 0xbe7d6156,
	0xbe7bdba4, 0xbe7a55cb, 0xbe78cfcc, 0xbe7749a6, 0xbe75c35a, 0xbe743ce8,
	0xbe72b651, 0xbe712f94, 0xbe6fa8b2, 0xbe6e21ab, 0xbe6c9a7f, 0xbe6b132f,
	0xbe698bba, 0xbe680422, 0xbe667c66, 0xbe64f486, 0xbe636c83, 0xbe61e45c,
	0xbe605c13, 0xbe5ed3a8, 0xbe5d4b1a, 0xbe5bc26a, 0xbe5a3997, 0xbe58b0a4,
	0xbe57278f, 0xbe559e58, 0xbe541501, 0xbe528b89, 0xbe5101f1, 0xbe4f7838,
	0xbe4dee60, 0xbe4c6467, 0xbe4ada4f, 0xbe495018, 0xbe47c5c2, 0xbe463b4d,
	0xbe44b0b9, 0xbe432607, 0xbe419b37, 0xbe401049, 0xbe3e853e, 0xbe3cfa15,
	0xbe3b6ecf, 0xbe39e36c, 0xbe3857ec, 0xbe36cc50, 0xbe354098, 0xbe33b4c4,
	0xbe3228d4, 0xbe309cc9, 0xbe2f10a2, 0xbe2d8461, 0xbe2bf804, 0xbe2a6b8d,
	0xbe28defc, 0xbe275251, 0xbe25c58c, 0xbe2438ad, 0xbe22abb6, 0xbe211ea5,
	0xbe1f917b, 0xbe1e0438, 0xbe1c76de, 0xbe1ae96b, 0xbe195be0, 0xbe17ce3d,
	0xbe164083, 0xbe14b2b2, 0xbe1324ca, 0xbe1196cc, 0xbe1008b7, 0xbe0e7a8b,
	0xbe0cec4a, 0xbe0b5df3, 0xbe09cf86, 0xbe084105, 0xbe06b26e, 0xbe0523c2,
	0xbe039502, 0xbe02062e, 0xbe007745, 0xbdfdd092, 0xbdfab273, 0xbdf7942c,
	0xbdf475c0, 0xbdf1572e, 0xbdee3876, 0xbdeb199a, 0xbde7fa9a, 0xbde4db76,
	0xbde1bc2e, 0xbdde9cc4, 0xbddb7d37, 0xbdd85d89, 0xbdd53db9, 0xbdd21dc8,
	0xbdcefdb7, 0xbdcbdd86, 0xbdc8bd36, 0xbdc59cc6, 0xbdc27c39, 0xbdbf5b8d,
	0xbdbc3ac3, 0xbdb919dd, 0xbdb5f8da, 0xbdb2d7bb, 0xbdafb680, 0xbdac952b,
	0xbda973ba, 0xbda65230, 0xbda3308c, 0xbda00ecf, 0xbd9cecf9, 0xbd99cb0a,
	0xbd96a905, 0xbd9386e7, 0xbd9064b4, 0xbd8d426a, 0xbd8a200a, 0xbd86fd94,
	0xbd83db0a, 0xbd80b86c, 0xbd7b2b74, 0xbd74e5e9, 0xbd6ea038, 0xbd685a62,
	0xbd621469, 0xbd5bce4c, 0xbd55880e, 0xbd4f41af, 0xbd48fb30, 0xbd42b491,
	0xbd3c6dd5, 0xbd3626fc, 0xbd2fe007, 0xbd2998f6, 0xbd2351cb, 0xbd1d0a88,
	0xbd16c32c, 0xbd107bb8, 0xbd0a342f, 0xbd03ec90, 0xbcfb49ba, 0xbceeba2c,
	0xbce22a7a, 0xbcd59aa6, 0xbcc90ab0, 0xbcbc7a9b, 0xbcafea69, 0xbca35a1c,
	0xbc96c9b6, 0xbc8a3938, 0xbc7b514b, 0xbc622fff, 0xbc490e90, 0xbc2fed02,
	0xbc16cb58, 0xbbfb5330, 0xbbc90f88, 0xbb96cbc1, 0xbb490fc6, 0xbac90fd5,
	0x00000000, 0x3ac90fd5, 0x3b490fc6, 0x3b96cbc1, 0x3bc90f88, 0x3bfb5330,
	0x3c16cb58, 0x3c2fed02, 0x3c490e90, 0x3c622fff, 0x3c7b514b, 0x3c8a3938,
	0x3c96c9b6, 0x3ca35a1c, 0x3cafea69, 0x3cbc7a9b, 0x3cc90ab0, 0x3cd59aa6,
	0x3ce22a7a, 0x3ceeba2c, 0x3cfb49ba, 0x3d03ec90, 0x3d0a342f, 0x3d107bb8,
	0x3d16c32c, 0x3d1d0a88, 0x3d2351cb, 0x3d2998f6, 0x3d2fe007, 0x3d3626fc,
	0x3d3c6dd5, 0x3d42b491, 0x3d48fb30, 0x3d4f41af, 0x3d55880e, 0x3d5bce4c,
	0x3d621469, 0x3d685a62, 0x3d6ea038, 0x3d74e5e9, 0x3d7b2b74, 0x3d80b86c,
	0x3d83db0a, 0x3d86fd94, 0x3d8a200a, 0x3d8d426a, 0x3d9064b4, 0x3d9386e7,
	0x3d96a905, 0x3d99cb0a, 0x3d9cecf9, 0x3da00ecf, 0x3da3308c, 0x3da65230,
	0x3da973ba, 0x3dac952b, 0x3dafb680, 0x3db2d7bb, 0x3db5f8da, 0x3db919dd,
	0x3dbc3ac3, 0x3dbf5b8d, 0x3dc27c39, 0x3dc59cc6, 0x3dc8bd36, 0x3dcbdd86,
	0x3dcefdb7, 0x3dd21dc8, 0x3dd53db9, 0x3dd85d89, 0x3ddb7d37, 0x3dde9cc4,
	0x3de1bc2e, 0x3de4db76, 0x3de7fa9a, 0x3deb199a, 0x3dee3876, 0x3df1572e,
	0x3df475c0, 0x3df7942c, 0x3dfab273, 0x3dfdd092, 0x3e007745, 0x3e02062e,
	0x3e039502, 0x3e0523c2, 0x3e06b26e, 0x3e084105, 0x3e09cf86, 0x3e0b5df3,
	0x3e0cec4a, 0x3e0e7a8b, 0x3e1008b7, 0x3e1196cc, 0x3e1324ca, 0x3e14b2b2,
	0x3e164083, 0x3e17ce3d, 0x3e195be0, 0x3e1ae96b, 0x3e1c76de, 0x3e1e0438,
	0x3e1f917b, 0x3e211ea5, 0x3e22abb6, 0x3e2438ad, 0x3e25c58c, 0x3e275251,
	0x3e28defc, 0x3e2a6b8d, 0x3e2bf804, 0x3e2d8461, 0x3e2f10a2, 0x3e309cc9,
	0x3e3228d4, 0x3e33b4c4, 0x3e354098, 0x3e36cc50, 0x3e3857ec, 0x3e39e36c,
	0x3e3b6ecf, 0x3e3cfa15, 0x3e3e853e, 0x3e401049, 0x3e419b37, 0x3e432607,
	0x3e44b0b9, 0x3e463b4d, 0x3e47c5c2, 0x3e495018, 0x3e4ada4f, 0x3e4c6467,
	0x3e4dee60, 0x3e4f7838, 0x3e5101f1, 0x3e528b89, 0x3e541501, 0x3e559e58,
	0x3e57278f, 0x3e58b0a4, 0x3e5a3997, 0x3e5bc26a, 0x3e5d4b1a, 0x3e5ed3a8,
	0x3e605c13, 0x3e61e45c, 0x3e636c83, 0x3e64f486, 0x3e667c66, 0x3e680422,
	0x3e698bba, 0x3e6b132f, 0x3e6c9a7f, 0x3e6e21ab, 0x3e6fa8b2, 0x3e712f94,
	0x3e72b651, 0x3e743ce8, 0x3e75c35a, 0x3e7749a6, 0x3e78cfcc, 0x3e7a55cb,
	0x3e7bdba4, 0x3e7d6156, 0x3e7ee6e1, 0x3e803622, 0x3e80f8c0, 0x3e81bb4a,
	0x3e827dc0, 0x3e834022, 0x3e840270, 0x3e84c4aa, 0x3e8586ce, 0x3e8648df,
	0x3e870ada, 0x3e87ccc1, 0x3e888e93, 0x3e895050, 0x3e8a11f7, 0x3e8ad38a,
	0x3e8b9507, 0x3e8c566e, 0x3e8d17c0, 0x3e8dd8fc, 0x3e8e9a22, 0x3e8f5b32,
	0x3e901c2c, 0x3e90dd10, 0x3e919ddd, 0x3e925e94, 0x3e931f35, 0x3e93dfbf,
	0x3e94a031, 0x3e95608d, 0x3e9620d2, 0x3e96e100, 0x3e97a117, 0x3e986116,
	0x3e9920fe, 0x3e99e0ce, 0x3e9aa086, 0x3e9b6027, 0x3e9c1faf, 0x3e9cdf20,
	0x3e9d9e78, 0x3e9e5db8, 0x3e9f1cdf, 0x3e9fdbee, 0x3ea09ae5, 0x3ea159c2,
	0x3ea21887, 0x3ea2d733, 0x3ea395c5, 0x3ea4543f, 0x3ea5129f, 0x3ea5d0e5,
	0x3ea68f12, 0x3ea74d25, 0x3ea80b1f, 0x3ea8c8fe, 0x3ea986c4, 0x3eaa446f,
	0x3eab0201, 0x3eabbf77, 0x3eac7cd4, 0x3ead3a15, 0x3eadf73c, 0x3eaeb449,
	0x3eaf713a, 0x3eb02e10, 0x3eb0eacb, 0x3eb1a76b, 0x3eb263ef, 0x3eb32058,
	0x3eb3dca5, 0x3eb498d6, 0x3eb554ec, 0x3eb610e6, 0x3eb6ccc3, 0x3eb78884,
	0x3eb8442a, 0x3eb8ffb2, 0x3eb9bb1e, 0x3eba766e, 0x3ebb31a0, 0x3ebbecb6,
	0x3ebca7af, 0x3ebd628b, 0x3ebe1d4a, 0x3ebed7eb, 0x3ebf926f, 0x3ec04cd5,
	0x3ec1071e, 0x3ec1c148, 0x3ec27b55, 0x3ec33544, 0x3ec3ef15, 0x3ec4a8c8,
	0x3ec5625c, 0x3ec61bd2, 0x3ec6d529, 0x3ec78e62, 0x3ec8477c, 0x3ec90077,
	0x3ec9b953, 0x3eca7210, 0x3ecb2aae, 0x3ecbe32c, 0x3ecc9b8b, 0x3ecd53ca,
	0x3ece0bea, 0x3ecec3ea, 0x3ecf7bca, 0x3ed0338a, 0x3ed0eb2a, 0x3ed1a2aa,
	0x3ed25a09, 0x3ed31148, 0x3ed3c867, 0x3ed47f64, 0x3ed53641, 0x3ed5ecfd,
	0x3ed6a399, 0x3ed75a13, 0x3ed8106b, 0x3ed8c6a3, 0x3ed97cb9, 0x3eda32ad,
	0x3edae880, 0x3edb9e31, 0x3edc53c1, 0x3edd092e, 0x3eddbe79, 0x3ede73a2,
	0x3edf28a9, 0x3edfdd8d, 0x3ee0924f, 0x3ee146ee, 0x3ee1fb6a, 0x3ee2afc4,
	0x3ee363fa, 0x3ee4180e, 0x3ee4cbfe, 0x3ee57fcb, 0x3ee63375, 0x3ee6e6fb,
	0x3ee79a5d, 0x3ee84d9c, 0x3ee900b7, 0x3ee9b3ae, 0x3eea6681, 0x3eeb1930,
	0x3eebcbbb, 0x3eec7e21, 0x3eed3063, 0x3eede280, 0x3eee9479, 0x3eef464c,
	0x3eeff7fb, 0x3ef0a985, 0x3ef15aea, 0x3ef20c29, 0x3ef2bd43, 0x3ef36e38,
	0x3ef41f07, 0x3ef4cfb1, 0x3ef58035, 0x3ef63093, 0x3ef6e0cb, 0x3ef790dc,
	0x3ef840c8, 0x3ef8f08e, 0x3ef9a02d, 0x3efa4fa5, 0x3efafef7, 0x3efbae22,
	0x3efc5d27, 0x3efd0c04, 0x3efdbabb, 0x3efe694a, 0x3eff17b2, 0x3effc5f3,
	0x3f003a06, 0x3f0090ff, 0x3f00e7e4, 0x3f013eb5, 0x3f019573, 0x3f01ec1c,
	0x3f0242b1, 0x3f029932, 0x3f02ef9f, 0x3f0345f8, 0x3f039c3d, 0x3f03f26d,
	0x3f044889, 0x3f049e91, 0x3f04f484, 0x3f054a62, 0x3f05a02c, 0x3f05f5e2,
	0x3f064b82, 0x3f06a10e, 0x3f06f686, 0x3f074be8, 0x3f07a136, 0x3f07f66f,
	0x3f084b92, 0x3f08a0a1, 0x3f08f59b, 0x3f094a7f, 0x3f099f4e, 0x3f09f409,
	0x3f0a48ad, 0x3f0a9d3d, 0x3f0af1b7, 0x3f0b461c, 0x3f0b9a6b, 0x3f0beea5,
	0x3f0c42c9, 0x3f0c96d7, 0x3f0cead0, 0x3f0d3eb3, 0x3f0d9281, 0x3f0de638,
	0x3f0e39da, 0x3f0e8d65, 0x3f0ee0db, 0x3f0f343b, 0x3f0f8784, 0x3f0fdab8,
	0x3f102dd5, 0x3f1080dc, 0x3f10d3cd, 0x3f1126a7, 0x3f11796b, 0x3f11cc19,
	0x3f121eb0, 0x3f127130, 0x3f12c39a, 0x3f1315ee, 0x3f13682a, 0x3f13ba50,
	0x3f140c5f, 0x3f145e58, 0x3f14b039, 0x3f150204, 0x3f1553b7, 0x3f15a554,
	0x3f15f6d9, 0x3f164847, 0x3f16999f, 0x3f16eade, 0x3f173c07, 0x3f178d18,
	0x3f17de12, 0x3f182ef5, 0x3f187fc0, 0x3f18d073, 0x3f19210f, 0x3f197194,
	0x3f19c200, 0x3f1a1255, 0x3f1a6293, 0x3f1ab2b8, 0x3f1b02c6, 0x3f1b52bb,
	0x3f1ba299, 0x3f1bf25f, 0x3f1c420c, 0x3f1c91a2, 0x3f1ce11f, 0x3f1d3084,
	0x3f1d7fd1, 0x3f1dcf06, 0x3f1e1e22, 0x3f1e6d26, 0x3f1ebc12, 0x3f1f0ae5,
	0x3f1f599f, 0x3f1fa841, 0x3f1ff6cb, 0x3f20453b, 0x3f209393, 0x3f20e1d2,
	0x3f212ff9, 0x3f217e06, 0x3f21cbfb, 0x3f2219d7, 0x3f226799, 0x3f22b543,
	0x3f2302d3, 0x3f23504b, 0x3f239da9, 0x3f23eaee, 0x3f24381a, 0x3f24852c,
	0x3f24d225, 0x3f251f04, 0x3f256bcb, 0x3f25b877, 0x3f26050a, 0x3f265184,
	0x3f269de3, 0x3f26ea2a, 0x3f273656, 0x3f278268, 0x3f27ce61, 0x3f281a40,
	0x3f286605, 0x3f28b1b0, 0x3f28fd41, 0x3f2948b8, 0x3f299415, 0x3f29df57,
	0x3f2a2a80, 0x3f2a758e, 0x3f2ac082, 0x3f2b0b5b, 0x3f2b561b, 0x3f2ba0bf,
	0x3f2beb4a, 0x3f2c35b9, 0x3f2c800f, 0x3f2cca49, 0x3f2d1469, 0x3f2d5e6f,
	0x3f2da859, 0x3f2df229, 0x3f2e3bde, 0x3f2e8578, 0x3f2ecef7, 0x3f2f185b,
	0x3f2f61a5, 0x3f2faad3, 0x3f2ff3e6, 0x3f303cde, 0x3f3085bb, 0x3f30ce7c,
	0x3f311722, 0x3f315fad, 0x3f31a81d, 0x3f31f071, 0x3f3238aa, 0x3f3280c7,
	0x3f32c8c9, 0x3f3310af, 0x3f33587a, 0x3f33a029, 0x3f33e7bc, 0x3f342f34,
	0x3f34768f, 0x3f34bdcf, 0x3f3504f3, 0x3f354bfb, 0x3f3592e7, 0x3f35d9b8,
	0x3f36206c, 0x3f366704, 0x3f36ad7f, 0x3f36f3df, 0x3f373a23, 0x3f37804a,
	0x3f37c655, 0x3f380c43, 0x3f385216, 0x3f3897cb, 0x3f38dd65, 0x3f3922e1,
	0x3f396842, 0x3f39ad85, 0x3f39f2ac, 0x3f3a37b7, 0x3f3a7ca4, 0x3f3ac175,
	0x3f3b0629, 0x3f3b4ac1, 0x3f3b8f3b, 0x3f3bd398, 0x3f3c17d9, 0x3f3c5bfc,
	0x3f3ca003, 0x3f3ce3ec, 0x3f3d27b8, 0x3f3d6b67, 0x3f3daef9, 0x3f3df26e,
	0x3f3e35c5, 0x3f3e78ff, 0x3f3ebc1b, 0x3f3eff1b, 0x3f3f41fc, 0x3f3f84c0,
	0x3f3fc767, 0x3f4009f0, 0x3f404c5c, 0x3f408ea9, 0x3f40d0da, 0x3f4112ec,
	0x3f4154e1, 0x3f4196b7, 0x3f41d870, 0x3f421a0b, 0x3f425b89, 0x3f429ce8,
	0x3f42de29, 0x3f431f4c, 0x3f436051, 0x3f43a138, 0x3f43e200, 0x3f4422ab,
	0x3f446337, 0x3f44a3a5, 0x3f44e3f5, 0x3f452426, 0x3f456439, 0x3f45a42d,
	0x3f45e403, 0x3f4623bb, 0x3f466354, 0x3f46a2ce, 0x3f46e22a, 0x3f472167,
	0x3f476085, 0x3f479f84, 0x3f47de65, 0x3f481d27, 0x3f485bca, 0x3f489a4e,
	0x3f48d8b3, 0x3f4916fa, 0x3f495521, 0x3f499329, 0x3f49d112, 0x3f4a0edc,
	0x3f4a4c87, 0x3f4a8a13, 0x3f4ac77f, 0x3f4b04cc, 0x3f4b41fa, 0x3f4b7f09,
	0x3f4bbbf8, 0x3f4bf8c7, 0x3f4c3578, 0x3f4c7208, 0x3f4cae79, 0x3f4ceacb,
	0x3f4d26fd, 0x3f4d6310, 0x3f4d9f02, 0x3f4ddad5, 0x3f4e1689, 0x3f4e521c,
	0x3f4e8d90, 0x3f4ec8e4, 0x3f4f0417, 0x3f4f3f2b, 0x3f4f7a1f, 0x3f4fb4f4,
	0x3f4fefa8, 0x3f502a3b, 0x3f5064af, 0x3f509f03, 0x3f50d937, 0x3f51134a,
	0x3f514d3d, 0x3f518710, 0x3f51c0c2, 0x3f51fa54, 0x3f5233c6, 0x3f526d18,
	0x3f52a649, 0x3f52df59, 0x3f531849, 0x3f535118, 0x3f5389c7, 0x3f53c255,
	0x3f53fac3, 0x3f54330f, 0x3f546b3b, 0x3f54a347, 0x3f54db31, 0x3f5512fb,
	0x3f554aa4, 0x3f55822c, 0x3f55b993, 0x3f55f0d9, 0x3f5627fe, 0x3f565f02,
	0x3f5695e5, 0x3f56cca7, 0x3f570348, 0x3f5739c7, 0x3f577026, 0x3f57a663,
	0x3f57dc7f, 0x3f581279, 0x3f584853, 0x3f587e0b, 0x3f58b3a1, 0x3f58e916,
	0x3f591e6a, 0x3f59539c, 0x3f5988ad, 0x3f59bd9c, 0x3f59f26a, 0x3f5a2716,
	0x3f5a5ba0, 0x3f5a9009, 0x3f5ac450, 0x3f5af875, 0x3f5b2c79, 0x3f5b605a,
	0x3f5b941a, 0x3f5bc7b8, 0x3f5bfb34, 0x3f5c2e8e, 0x3f5c61c7, 0x3f5c94dd,
	0x3f5cc7d1, 0x3f5cfaa3, 0x3f5d2d53, 0x3f5d5fe1, 0x3f5d924d, 0x3f5dc497,
	0x3f5df6be, 0x3f5e28c3, 0x3f5e5aa6, 0x3f5e8c67, 0x3f5ebe05, 0x3f5eef81,
	0x3f5f20db, 0x3f5f5212, 0x3f5f8327, 0x3f5fb419, 0x3f5fe4e9, 0x3f601596,
	0x3f604621, 0x3f607689, 0x3f60a6cf, 0x3f60d6f2, 0x3f6106f2, 0x3f6136d0,
	0x3f61668a, 0x3f619622, 0x3f61c598, 0x3f61f4ea, 0x3f62241a, 0x3f625326,
	0x3f628210, 0x3f62b0d7, 0x3f62df7b, 0x3f630dfc, 0x3f633c5a, 0x3f636a95,
	0x3f6398ac, 0x3f63c6a1, 0x3f63f473, 0x3f642221, 0x3f644fac, 0x3f647d14,
	0x3f64aa59, 0x3f64d77b, 0x3f650479, 0x3f653154, 0x3f655e0b, 0x3f658aa0,
	0x3f65b710, 0x3f65e35e, 0x3f660f88, 0x3f663b8e, 0x3f666771, 0x3f669330,
	0x3f66becc, 0x3f66ea45, 0x3f671599, 0x3f6740ca, 0x3f676bd8, 0x3f6796c1,
	0x3f67c187, 0x3f67ec29, 0x3f6816a8, 0x3f684103, 0x3f686b39, 0x3f68954c,
	0x3f68bf3c, 0x3f68e907, 0x3f6912ae, 0x3f693c32, 0x3f696591, 0x3f698ecc,
	0x3f69b7e4, 0x3f69e0d7, 0x3f6a09a7, 0x3f6a3252, 0x3f6a5ad9, 0x3f6a833c,
	0x3f6aab7b, 0x3f6ad395, 0x3f6afb8c, 0x3f6b235e, 0x3f6b4b0c, 0x3f6b7295,
	0x3f6b99fb, 0x3f6bc13b, 0x3f6be858, 0x3f6c0f50, 0x3f6c3624, 0x3f6c5cd4,
	0x3f6c835e, 0x3f6ca9c5, 0x3f6cd007, 0x3f6cf624, 0x3f6d1c1d, 0x3f6d41f2,
	0x3f6d67a1, 0x3f6d8d2d, 0x3f6db293, 0x3f6dd7d5, 0x3f6dfcf2, 0x3f6e21eb,
	0x3f6e46be, 0x3f6e6b6d, 0x3f6e8ff8, 0x3f6eb45d, 0x3f6ed89e, 0x3f6efcba,
	0x3f6f20b0, 0x3f6f4483, 0x3f6f6830, 0x3f6f8bb8, 0x3f6faf1b, 0x3f6fd25a,
	0x3f6ff573, 0x3f701867, 0x3f703b37, 0x3f705de1, 0x3f708066, 0x3f70a2c6,
	0x3f70c501, 0x3f70e717, 0x3f710908, 0x3f712ad4, 0x3f714c7a, 0x3f716dfb,
	0x3f718f57, 0x3f71b08e, 0x3f71d19f, 0x3f71f28c, 0x3f721352, 0x3f7233f4,
	0x3f725470, 0x3f7274c7, 0x3f7294f8, 0x3f72b504, 0x3f72d4eb, 0x3f72f4ac,
	0x3f731447, 0x3f7333be, 0x3f73530e, 0x3f737239, 0x3f73913f, 0x3f73b01f,
	0x3f73ced9, 0x3f73ed6e, 0x3f740bdd, 0x3f742a27, 0x3f74484b, 0x3f746649,
	0x3f748422, 0x3f74a1d5, 0x3f74bf62, 0x3f74dcc9, 0x3f74fa0b, 0x3f751727,
	0x3f75341d, 0x3f7550ed, 0x3f756d97, 0x3f758a1c, 0x3f75a67b, 0x3f75c2b3,
	0x3f75dec6, 0x3f75fab3, 0x3f76167a, 0x3f76321b, 0x3f764d97, 0x3f7668ec,
	0x3f76841b, 0x3f769f24, 0x3f76ba07, 0x3f76d4c4, 0x3f76ef5b, 0x3f7709cc,
	0x3f772417, 0x3f773e3c, 0x3f77583a, 0x3f777213, 0x3f778bc5, 0x3f77a551,
	0x3f77beb7, 0x3f77d7f7, 0x3f77f110, 0x3f780a04, 0x3f7822d1, 0x3f783b77,
	0x3f7853f8, 0x3f786c52, 0x3f788486, 0x3f789c93, 0x3f78b47b, 0x3f78cc3b,
	0x3f78e3d6, 0x3f78fb4a, 0x3f791298, 0x3f7929bf, 0x3f7940c0, 0x3f79579a,
	0x3f796e4e, 0x3f7984dc, 0x3f799b43, 0x3f79b183, 0x3f79c79d, 0x3f79dd91,
	0x3f79f35e, 0x3f7a0904, 0x3f7a1e84, 0x3f7a33dd, 0x3f7a4910, 0x3f7a5e1c,
	0x3f7a7302, 0x3f7a87c1, 0x3f7a9c59, 0x3f7ab0cb, 0x3f7ac516, 0x3f7ad93a,
	0x3f7aed37, 0x3f7b010e, 0x3f7b14be, 0x3f7b2848, 0x3f7b3bab, 0x3f7b4ee7,
	0x3f7b61fc, 0x3f7b74ea, 0x3f7b87b2, 0x3f7b9a53, 0x3f7baccd, 0x3f7bbf20,
	0x3f7bd14d, 0x3f7be353, 0x3f7bf531, 0x3f7c06e9, 0x3f7c187a, 0x3f7c29e5,
	0x3f7c3b28, 0x3f7c4c44, 0x3f7c5d3a, 0x3f7c6e08, 0x3f7c7eb0, 0x3f7c8f31,
	0x3f7c9f8a, 0x3f7cafbd, 0x3f7cbfc9, 0x3f7ccfae, 0x3f7cdf6c, 0x3f7cef03,
	0x3f7cfe73, 0x3f7d0dbc, 0x3f7d1cdd, 0x3f7d2bd8, 0x3f7d3aac, 0x3f7d4959,
	0x3f7d57de, 0x3f7d663d, 0x3f7d7474, 0x3f7d8285, 0x3f7d906e, 0x3f7d9e30,
	0x3f7dabcc, 0x3f7db940, 0x3f7dc68c, 0x3f7dd3b2, 0x3f7de0b1, 0x3f7ded88,
	0x3f7dfa38, 0x3f7e06c2, 0x3f7e1324, 0x3f7e1f5e, 0x3f7e2b72, 0x3f7e375e,
	0x3f7e4323, 0x3f7e4ec1, 0x3f7e5a38, 0x3f7e6588, 0x3f7e70b0, 0x3f7e7bb1,
	0x3f7e868b, 0x3f7e913d, 0x3f7e9bc9, 0x3f7ea62d, 0x3f7eb069, 0x3f7eba7f,
	0x3f7ec46d, 0x3f7ece34, 0x3f7ed7d4, 0x3f7ee14c, 0x3f7eea9d, 0x3f7ef3c7,
	0x3f7efcc9, 0x3f7f05a4, 0x3f7f0e58, 0x3f7f16e4, 0x3f7f1f49, 0x3f7f2787,
	0x3f7f2f9d, 0x3f7f378c, 0x3f7f3f54, 0x3f7f46f4, 0x3f7f4e6d, 0x3f7f55bf,
	0x3f7f5ce9, 0x3f7f63ec, 0x3f7f6ac7, 0x3f7f717b, 0x3f7f7808, 0x3f7f7e6d,
	0x3f7f84ab, 0x3f7f8ac2, 0x3f7f90b1, 0x3f7f9678, 0x3f7f9c18, 0x3f7fa191,
	0x3f7fa6e3, 0x3f7fac0d, 0x3f7fb10f, 0x3f7fb5ea, 0x3f7fba9e, 0x3f7fbf2a,
	0x3f7fc38f, 0x3f7fc7cc, 0x3f7fcbe2, 0x3f7fcfd1, 0x3f7fd397, 0x3f7fd737,
	0x3f7fdaaf, 0x3f7fde00, 0x3f7fe129, 0x3f7fe42b, 0x3f7fe705, 0x3f7fe9b8,
	0x3f7fec43, 0x3f7feea7, 0x3f7ff0e3, 0x3f7ff2f8, 0x3f7ff4e6, 0x3f7ff6ac,
	0x3f7ff84a, 0x3f7ff9c1, 0x3f7ffb11, 0x3f7ffc39, 0x3f7ffd39, 0x3f7ffe13,
	0x3f7ffec4, 0x3f7fff4e, 0x3f7fffb1, 0x3f7fffec, 0x3f800000,
};

// TODO: Is this correct in all platforms/architectures? (32 vs 64 bits)
float *SinTabF = (float *)SinTabU32;
float *CosTabF = (float *)CosTabU32;
