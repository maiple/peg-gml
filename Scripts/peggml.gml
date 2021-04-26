#define main
// Generated at 2021-04-25 19:15:09 (264ms) for v1.4.1804+
PegGML_main();

//{ PegGML

#define PegGML_main
// PegGML_main()
trace("PegGML.hx:3:", "Hello World!");

//}

//{ haxe.boot

#define haxe_boot_decl
// haxe_boot_decl(...values:T)->array<T>
var _n = argument_count;
var _i, _r;
if (_n == 0) {
	_r = undefined;
	_r[1, 0] = undefined;
	return _r;
}
if (os_browser != browser_not_a_browser) {
	_r = undefined;
	_r[@0] = argument[0];
	_i = 0;
	while (++_i < _n) {
		_r[_i] = argument[_i];
	}
} else {
	_r = undefined;
	while (--_n >= 0) {
		_r[_n] = argument[_n];
	}
}
return _r;

//}
