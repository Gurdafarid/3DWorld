varying vec4 epos;

void main()
{
	gl_FragColor = apply_fog_epos(gl_Color, epos);
}
