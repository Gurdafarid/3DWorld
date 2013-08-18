varying vec3 world_space_pos;
varying vec4 epos;

void main()
{
	float alpha = 1.0/(1.0 + 40.0*length(epos.xyz)); // attenuate when far from the camera
	if (alpha < 0.01) {discard;}
	vec3 normal = normalize(-epos.xyz); // facing the camera
	vec4 color  = gl_FrontMaterial.emission;
	float atten[2] = {calc_shadow_atten(world_space_pos), 1.0};

	for (int i = 0; i < 2; ++i) { // sun_diffuse, galaxy_ambient
		color += atten[i]*add_pt_light_comp(normal, epos, i);
	}
	gl_FragColor = vec4(color.rgb, alpha * gl_Color.a); // use diffuse alpha directly;
}

