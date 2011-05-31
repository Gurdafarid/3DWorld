// 3D World - Shape primitive drawing
// by Frank Gennari
// 5/1/05
#include "3DWorld.h"
#include "mesh.h"
#include "csg.h"


bool const VOXEL_MERGE        = 0;
bool const SUB_CUBE_MERGE     = 1;
bool const UNOVERLAP_COBJS    = 1;
bool const MERGE_COBJS        = 1;
bool const CHECK_COBJS        = 1;
bool const EFLAGS_STRICT      = 0;
bool const ONLY_SUB_PREV_NEG  = 1;
bool const CHECK_ADJACENCY    = 0; // doesn't seem to make any significant difference
int  const REMOVE_T_JUNCTIONS = 1; // fewer hole pixels and better ambient transitions but more cobjs and render time
float const REL_DMAX          = 0.2;

extern vector<portal> portals;


// *** RECT IMPLEMENTATION ***


int destroy_thresh(0);


rect::rect(float const r[3][2], unsigned d0, unsigned d1) { // projection from 3D => 2D

	assert(d0 >= 0 && d0 <= 3 && d1 >= 0 && d1 <= 3 && d0 != d1);
	d[0][0] = r[d0][0]; d[0][1] = r[d0][1]; d[1][0] = r[d1][0]; d[1][1] = r[d1][1];
	//assert(nonzero());
}


void rect::clip_to(float const c[2][2]) {

	for (unsigned i = 0; i < 2; ++i) {
		assert(c[i][0] < c[i][1]);
		d[i][0] = max(c[i][0], min(c[i][1], d[i][0]));
		d[i][1] = max(c[i][0], min(c[i][1], d[i][1]));
	}
}


// rr will be removed
void rect::subtract_from(rect const &rr, deque<rect> &new_rects) const { // subtract ourself from rr

	if (contains(rr.d)) return;
	unsigned i2[2], n[2];
	char vox[3][3]; // 3x3 pixels
	float vals[2][4];
	rect r2;

	// determine cutting planes
	for (unsigned i = 0; i < 2; ++i) {
		n[i]       = 0;
		vals[i][0] = rr.d[i][0];

		for (unsigned j = 0; j < 2; ++j) {
			if (d[i][j] > rr.d[i][0] && d[i][j] < rr.d[i][1]) vals[i][++n[i]] = d[i][j];
		}
		vals[i][++n[i]] = rr.d[i][1];
	}

	// build pixel table
	for (i2[0] = 0; i2[0] < n[0]; ++i2[0]) {
		for (i2[1] = 0; i2[1] < n[1]; ++i2[1]) {
			float pt[2];
			for (unsigned l = 0; l < 2; ++l) { // pt is the center of this pixel
				pt[l] = 0.5*(vals[l][i2[l]] + vals[l][i2[l]+1]);
			}
			vox[i2[0]][i2[1]] = !contains_pt(pt);
		}
	}

	// merge adjacent pixels (optional performance improvement)
	for (unsigned dim = 0; dim < 2; ++dim) { // rotate the cube slicing direction
		unsigned const d1(!dim);

		for (i2[dim] = 0; i2[dim] < n[dim]; ++i2[dim]) {
			bool all_in(1);

			for (i2[d1] = 0; i2[d1] < n[d1] && all_in; ++i2[d1]) {
				if (!vox[i2[0]][i2[1]]) all_in = 0;
			}
			if (all_in) {
				for (unsigned l = 0; l < 2; ++l) {
					for (unsigned m = 0; m < 2; ++m) {
						r2.d[l][m] = ((l == dim) ? vals[l][i2[l]+m] : rr.d[l][m]); // slice dimensions
					}
				}
				if (!r2.is_near_zero_area()) new_rects.push_back(r2);

				for (i2[d1] = 0; i2[d1] < n[d1]; ++i2[d1]) {
					vox[i2[0]][i2[1]] = 0; // remove pixel
				}
			}
		}
	}

	// generate output rects
	for (i2[0] = 0; i2[0] < n[0]; ++i2[0]) {
		for (i2[1] = 0; i2[1] < n[1]; ++i2[1]) {
			if (vox[i2[0]][i2[1]]) {
				for (unsigned l = 0; l < 2; ++l) {
					for (unsigned m = 0; m < 2; ++m) {
						r2.d[l][m] = vals[l][i2[l]+m];
					}
				}
				if (!r2.is_near_zero_area()) new_rects.push_back(r2);
			}
		}
	}
}


bool rect::merge_with(rect const &r) {

	for (unsigned j = 0; j < 2; ++j) {
		if (r.d[j][0] == d[j][0] && r.d[j][1] == d[j][1]) {
			for (unsigned k = 0; k < 2; ++k) {
				if (r.d[!j][!k] == d[!j][k]) {
					d[!j][k] = r.d[!j][k];
					return 1;
				}
			}
		}
	}
	return 0;
}


void rect::print() const {

	for (unsigned i = 0; i < 2; ++i) {
		for (unsigned j = 0; j < 2; ++j) {
			cout << d[i][j];
			if (j == 0) cout << " ";
		}
		if (i == 0) cout << ", ";
	}
}


// *** CUBE_T IMPLEMENTATION ***


void cube_t::set_from_points(point const *const pts, unsigned npts) {

	assert(npts > 0);
	UNROLL_3X(d[i_][0] = d[i_][1] = pts[0][i_];)
	
	for (unsigned i = 1; i < npts; ++i) { // get bounding xy rectangle
		union_with_pt(pts[i]);
	}
}


void cube_t::print() const {

	for (unsigned i = 0; i < 3; ++i) {
		for (unsigned j = 0; j < 2; ++j) {
			cout << d[i][j] << ",";
		}
		cout << " ";
	}
}


bool cube_t::is_near_zero_area() const {
		
	UNROLL_3X(if (fabs(d[i_][0] - d[i_][1]) < TOLER) return 1;)
	return 0;
}


bool cube_t::cube_intersection(const cube_t &cube, cube_t &res) const { // flags are not set
	
	for (unsigned i = 0; i < 3; ++i) {
		res.d[i][0] = max(d[i][0], cube.d[i][0]);
		res.d[i][1] = min(d[i][1], cube.d[i][1]);
		if (res.d[0] >= res.d[1]) return 0; // no intersection
	}
	return 1;
}


vector3d cube_t::closest_side_dir(point const &pos) const { // for fragment velocity

	int dir(-1);
	float mdist(0.0);
	vector3d dv(zero_vector);

	for (unsigned i = 0; i < 3; ++i) {
		for (unsigned j = 0; j < 2; ++j) {
			float const dist(fabs(d[i][j] - pos[i]));

			if (dir == -1 || dist < mdist) {
				mdist = dist;
				dir   = (i << 1) + j;
			}
		}
	}
	dv[dir >> 1] = ((dir & 1) ? 1.0 : -1.0);
	return dv;
}


// *** CSG_CUBE IMPLEMENTATION ***


// returns 1 if entire cube is removed
bool csg_cube::subtract_from_internal(const csg_cube &cube, vector<csg_cube> &output) const { // subtract ourself from cube

	if (contains_cube(cube)) return 1;
	unsigned const u[3][3] = {{1,0,0}, {0,1,0}, {0,0,1}}; // unit vectors
	unsigned i3[3], n[3];
	char vox[3][3][3]; // 3x3x3 voxels
	float vals[3][4];

	// determine cutting planes
	for (unsigned i = 0; i < 3; ++i) {
		n[i]       = 0;
		vals[i][0] = cube.d[i][0];

		for (unsigned j = 0; j < 2; ++j) {
			if (d[i][j] > (cube.d[i][0] + TOLER) && d[i][j] < (cube.d[i][1] - TOLER)) vals[i][++n[i]] = d[i][j];
		}
		vals[i][++n[i]] = cube.d[i][1];
	}

	// build voxel table
	for (i3[0] = 0; i3[0] < n[0]; ++i3[0]) {
		for (i3[1] = 0; i3[1] < n[1]; ++i3[1]) {
			for (i3[2] = 0; i3[2] < n[2]; ++i3[2]) {
				point pt;
				for (unsigned l = 0; l < 3; ++l) { // pt is the center of this voxel
					pt[l] = 0.5*(vals[l][i3[l]] + vals[l][i3[l]+1]);
				}
				vox[i3[0]][i3[1]][i3[2]] = !contains_pt(pt);
			}
		}
	}

	// merge adjacent voxels
	if (VOXEL_MERGE) {
		for (unsigned dim = 0; dim < 3; ++dim) { // rotate the cube slicing direction
			unsigned const d1((dim+1)%3), d2((dim+2)%3);

			for (i3[dim] = 0; i3[dim] < n[dim]; ++i3[dim]) {
				bool all_in(1);

				for (i3[d1] = 0; i3[d1] < n[d1] && all_in; ++i3[d1]) {
					for (i3[d2] = 0; i3[d2] < n[d2] && all_in; ++i3[d2]) {
						if (!vox[i3[0]][i3[1]][i3[2]]) all_in = 0;
					}
				}
				if (all_in) { // full 2D slice
					unsigned char edgeflags(cube.eflags); // |= vs. &=~ ???
					if (i3[dim] != 0)        edgeflags &= ~EFLAGS[dim][0]; // first  side not on original cube edge
					if (i3[dim] != n[dim]-1) edgeflags &= ~EFLAGS[dim][1]; // second side not on original cube edge
					csg_cube ncube(edgeflags);

					for (unsigned l = 0; l < 3; ++l) {
						for (unsigned m = 0; m < 2; ++m) {
							ncube.d[l][m] = ((l == dim) ? vals[l][i3[l]+m] : cube.d[l][m]); // slice dimensions
						}
					}
					output.push_back(ncube);

					for (i3[d1] = 0; i3[d1] < n[d1]; ++i3[d1]) {
						for (i3[d2] = 0; i3[d2] < n[d2]; ++i3[d2]) {
							vox[i3[0]][i3[1]][i3[2]] = 0; // remove voxel
						}
					}
				}
			}
		}
	}

	// generate output cubes
	for (i3[0] = 0; i3[0] < n[0]; ++i3[0]) {
		for (i3[1] = 0; i3[1] < n[1]; ++i3[1]) {
			for (i3[2] = 0; i3[2] < n[2]; ++i3[2]) {
				if (!vox[i3[0]][i3[1]][i3[2]]) continue;
				unsigned char edgeflags(0);

				for (unsigned l = 0; l < 3; ++l) { // set edge flags to remove inside faces
					assert(n[l] > 0 && n[l] <= 3);
					if (i3[l] > 0) {
						if (vox[i3[0]-u[0][l]][i3[1]-u[1][l]][i3[2]-u[2][l]]) edgeflags |= EFLAGS[l][0];
					}
					else edgeflags |= (cube.eflags & EFLAGS[l][0]);
					
					if (i3[l] < n[l]-1) {
						if (vox[i3[0]+u[0][l]][i3[1]+u[1][l]][i3[2]+u[2][l]]) edgeflags |= EFLAGS[l][1];
					}
					else edgeflags |= (cube.eflags & EFLAGS[l][1]);
				}
				csg_cube ncube(edgeflags);
				
				for (unsigned l = 0; l < 3; ++l) {
					for (unsigned m = 0; m < 2; ++m) {
						ncube.d[l][m] = vals[l][i3[l]+m];
					}
				}
				if (SUB_CUBE_MERGE) {
					bool merged(0);
					for (unsigned c = 0; c < output.size() && !merged; ++c) {
						if (output[c].cube_merge(ncube, 1)) merged = 1;
					}
					if (merged) continue;
				}
				if (!ncube.is_near_zero_area()) output.push_back(ncube);
			}
		}
	}
	return 0;
}


csg_cube::csg_cube(const coll_obj &cobj, bool use_bounding_cube) : eflags(cobj.cp.surfs) { // coll_obj constructor

	assert(use_bounding_cube || cobj.type == COLL_CUBE || cobj.is_cylinder());
	copy_from(cobj);
	normalize();
}


inline void csg_cube::write_to_cobj(coll_obj &cobj) const {

	assert(cobj.type == COLL_CUBE);
	cobj.copy_from(*this);
	cobj.cp.surfs = eflags;
}


bool csg_cube::cube_intersection(const csg_cube &cube, csg_cube &res) const { // flags are not set

	res.eflags = 0; // fix later
	return cube_t::cube_intersection(cube, res);
}


// returns 1 if some work is done
bool csg_cube::subtract_from_cube(vector<coll_obj> &new_cobjs, coll_obj const &cobj) const { // subtract ourself from cobjs[index]

	assert(cobj.type == COLL_CUBE);
	if (!quick_intersect_test(cobj)) return 0; // no intersection
	csg_cube cube(cobj);
	if (!intersects(cube, TOLER))    return 0; // no intersection
	if (cube.is_zero_area())         return 1; // if zero area then remove it entirely
	vector<csg_cube> output;
	subtract_from_internal(cube, output);
	
	for (unsigned i = 0; i < output.size(); ++i) {
		new_cobjs.push_back(cobj); // keep properties of old coll cube
		output[i].write_to_cobj(new_cobjs.back());
		//assert(!intersects(new_cobjs.back()));
	}
	return 1;
}


// returns 1 if some work is done
bool csg_cube::subtract_from_cylinder(vector<coll_obj> &new_cobjs, coll_obj &cobj) const { // subtract ourself from cobjs[index]

	assert(cobj.is_cylinder());
	float const radius(max(cobj.radius, cobj.radius2)); // containment/intersection tests are conservative
	point const &p0(cobj.points[0]), &p1(cobj.points[1]);

	for (unsigned p = 0; p < 3; ++p) { // m,n,p dimensions
		unsigned const m((p+1)%3), n((p+2)%3);
		if (p0[m] != p1[m] || p0[n] != p1[n]) continue;
		assert(p0[p] != p1[p]);
		
		if (p0[p] > p1[p]) {
			swap(cobj.points[0], cobj.points[1]); // upside down (note that p0/p1 are also swapped)
			swap(cobj.radius,    cobj.radius2);
		}
		float const c[2][2] = {{p0[m]-radius, p0[m]+radius}, {p0[n]-radius, p0[n]+radius}};
		if (c[0][0] <  d[m][0] || c[0][1] >  d[m][1]) return 0; // no m-containment
		if (c[1][0] <  d[n][0] || c[1][1] >  d[n][1]) return 0; // no n-containment
		if (p0[p]   >= d[p][1] || p1[p]   <= d[p][0]) return 0; // no p-intersection
		if (p0[p]   >= d[p][0] && p1[p]   <= d[p][1]) return 1; // p-containment - remove
		csg_cube const cube(cobj);
		if (cube.is_zero_area()) return 1; // if zero area then remove it entirely
		cobj.points[0][m] = cobj.points[1][m]; // update cobj.d?
		cobj.points[0][n] = cobj.points[1][n];
		unsigned nv(0);
		float vals[4];

		if (p0[p] < d[p][0]) {
			vals[nv++] = p0[p];   // A
			vals[nv++] = d[p][0]; // B
		}
		if (p1[p] > d[p][1]) {
			vals[nv++] = d[p][1]; // C
			vals[nv++] = p1[p];   // D
		}
		for (unsigned i = 0; i < nv; i += 2) {
			new_cobjs.push_back(cobj); // keep properties of old coll cylinder
			new_cobjs.back().cp.surfs = 0; // reset edge flags in case the ends become exposed
			
			for (unsigned j = 0; j < 2; ++j) {
				new_cobjs.back().points[j][p] = vals[i+j];
			}
			if (cobj.radius != cobj.radius2) { // calculate new radius values
				float rv[2];

				for (unsigned j = 0; j < 2; ++j) {
					rv[j] = cobj.radius + (cobj.radius2 - cobj.radius)*(vals[i+j] - p0[p])/(p1[p] - p0[p]);
				}
				new_cobjs.back().radius  = rv[0];
				new_cobjs.back().radius2 = rv[1];
			}
		}
		return 1;
	} // for p
	return 0;
}


float get_cube_dmax() {

	return REL_DMAX*(X_SCENE_SIZE + Y_SCENE_SIZE);
}


// could do this dynamically as cubes are split
bool csg_cube::cube_merge(csg_cube &cube, bool proc_eflags) {

	unsigned compat[3], nc(0), ci(0);

	for (unsigned i = 0; i < 3; ++i) { // check compatability
		compat[i] = 1;
		for (unsigned j = 0; j < 2; ++j) {
			if (cube.d[i][j] != d[i][j]) compat[i] = 0;
			else if (EFLAGS_STRICT && proc_eflags && ((cube.eflags ^ eflags) & EFLAGS[i][j])) compat[i] = 0;
		}
		if (compat[i]) ++nc; else ci = i;
	}
	if (nc >= 2) { // compatible
		if (nc == 3) { // same cube, remove it
			if (proc_eflags) {
				for (unsigned i = 0; i < 3; ++i) { // merge edge flags
					for (unsigned j = 0; j < 2; ++j) {
						if (!(cube.eflags & EFLAGS[i][j])) eflags &= ~EFLAGS[i][j];
					}
				}
			}
			return 1;
		}
		float const dmax(get_cube_dmax());

		for (unsigned i = 0; i < 2; ++i) { // only one iteration will merge something
			if (cube.d[ci][1-i] == d[ci][i]) { // adjacent
				if (proc_eflags) { // only size test for real eflags cubes
					float const dval(fabs(cube.d[ci][i] - d[ci][1-i]));
					if (dval > dmax) return 0; // resulting cube will be too large
				}
				d[ci][i] = cube.d[ci][i];

				if (proc_eflags) {
					eflags  &= ~EFLAGS[ci][i];
					eflags  |= (cube.eflags & EFLAGS[ci][i]);

					for (unsigned j = 0; j < 3; ++j) { // set shared edge flags
						if (j != ci) {
							for (unsigned k = 0; k < 2; ++k) {
								if (!(cube.eflags & EFLAGS[j][k])) eflags &= ~EFLAGS[j][k];
							}
						}
					}
				}
				return 1;
			}
		}
	}
	if (CHECK_ADJACENCY && proc_eflags /*&& nc == 1*/) { // no merge, determine contained faces (does gridbag need overlap to catch all of these cases?)
		bool adjacent(0);

		for (unsigned i = 0; i < 3 && !adjacent; ++i) { // could we use ci or something?
			for (unsigned j = 0; j < 2 && !adjacent; ++j) {
				if (cube.d[i][j] == d[i][!j]) { // potential adjacency
					adjacent = 1;
					bool contained[2] = {1, 1}; // cube,self = {c,s}
					for (unsigned k = 0; k < 3; ++k) {
						if (k == i) continue;
						for (unsigned l = 0; l < 2; ++l) {
							if (cube.d[k][l] < d[k][l]) contained[ l] = 0;
							if (cube.d[k][l] > d[k][l]) contained[!l] = 0;
						}
					}
					assert(!(contained[0] && contained[1])); // nc should be at least 2 if we get here
					
					if (contained[0]) {
						cube.eflags |= EFLAGS[i][j];
					}
					else if (contained[1]) {
						eflags |= EFLAGS[i][!j];
					}
				}
			}
		}
	}
	return 0;
}


void csg_cube::unset_adjacent_edge_flags(coll_obj &cobj) const {

	for (unsigned i = 0; i < 3; ++i) {
		for (unsigned j = 0; j < 2; ++j) {
			if (fabs(d[i][j] - cobj.d[i][!j]) < TOLER) {
				bool overlaps(1);

				for (unsigned k = 0; k < 3 && !overlaps; ++k) {
					if (k != i && (d[k][0] >= cobj.d[k][1] || d[k][1] <= cobj.d[k][0])) overlaps = 0;
				}
				if (overlaps) {
					cobj.cp.surfs &= ~EFLAGS[i][!j];
					return;
				}
			}
		}
	}
}


// *** CSG ALGORITHM CODE ***


void get_cube_points(const float d[3][2], point pts[8]) {

	unsigned i[3];

	for (i[0] = 0; i[0] < 2; ++i[0]) {
		for (i[1] = 0; i[1] < 2; ++i[1]) {
			for (i[2] = 0; i[2] < 2; ++i[2]) {
				UNROLL_3X(pts[(((i[0]<<1)+i[1])<<1)+i[2]][i_] = d[i_][i[i_]];)
			}
		}
	}
}


void remove_invalid_cobjs(vector<coll_obj> &cobjs) {

	vector<coll_obj> cobjs2;
	unsigned const ncobjs(cobjs.size());

	for (unsigned i = 0; i < ncobjs; ++i) { // create new shapes vector with bad shapes removed
		if (cobjs[i].type != COLL_INVALID) cobjs2.push_back(cobjs[i]);
	}
	cobjs.swap(cobjs2);
}


void check_cubes(vector<coll_obj> &cobjs) {

	if (!CHECK_COBJS) return;
	unsigned const ncobjs(cobjs.size());

	for (unsigned i = 0; i < ncobjs; ++i) {
		if (cobjs[i].type != COLL_CUBE) continue;
		csg_cube const cube(cobjs[i]);
		
		if (cube.is_zero_area()) {
		  cout << "Zero area cube: "; cube.print(); cout << endl;
		  assert(0);
		}
	}
}


bool comp_by_params(const coll_obj &A, const coll_obj &B) {

	bool const sta(A.is_semi_trans()), stb(B.is_semi_trans()); // compare transparency first so that alpha blending works
	if (sta        < stb       ) return 1;
	if (stb        < sta       ) return 0;
	if (A.cp.color < B.cp.color) return 1;
	if (B.cp.color < A.cp.color) return 0;
	if (A.cp.tid   < B.cp.tid)   return 1;
	if (B.cp.tid   < A.cp.tid)   return 0;
	if (A.type     < B.type)     return 1;
	if (B.type     < A.type)     return 0;
	if (A.cp.draw  < B.cp.draw)  return 1;
	if (B.cp.draw  < A.cp.draw)  return 0;
	if (A.status   < B.status)   return 1;
	if (B.status   < A.status)   return 0;
	return (A.cp.elastic < B.cp.elastic);
}


// Note: also sorts by alpha so that transparency works correctly
void merge_cubes(vector<coll_obj> &cobjs) { // only merge compatible cubes

	if (!MERGE_COBJS) return;
	RESET_TIME;
	unsigned const ncobjs(cobjs.size());
	unsigned merged(0);
	vector<unsigned> type_blocks;

	// sorting can permute cobjs so that their id's are not monotonically increasing
	sort(cobjs.begin(), cobjs.end(), comp_by_params); // how does ordering affect drawing?

	for (unsigned i = 0; i < ncobjs; ++i) {
		if (i == 0 || !cobjs[i].equal_params(cobjs[i-1])) {
			type_blocks.push_back(i); // first cobj of a new param type
		}
	}
	type_blocks.push_back(ncobjs);

	for (unsigned i = 0, curr_tb = 0; i < ncobjs; ++i) {
		if (i == type_blocks[curr_tb+1]) {
			++curr_tb;
			assert(curr_tb+1 < type_blocks.size());
		}
		if (cobjs[i].type != COLL_CUBE) continue;
		csg_cube cube(cobjs[i]);
		if (cube.is_zero_area()) continue;
		unsigned mi(0);
		unsigned const j1(type_blocks[curr_tb]), j2(type_blocks[curr_tb+1]);

		for (unsigned j = j1; j < j2; ++j) {
			if (j == i || cobjs[j].type != COLL_CUBE) continue;
			//assert(cobjs[i].equal_params(cobjs[j]));
			csg_cube cube2(cobjs[j]);

			if (cube.cube_merge(cube2, 1)) {
				cobjs[j].type = COLL_INVALID; // remove old coll obj
				++mi;
			}
		}
		if (mi > 0) { // cube has changed
			cube.write_to_cobj(cobjs[i]);
			merged += mi;
		}
	}
	if (merged > 0) remove_invalid_cobjs(cobjs);
	cout << ncobjs << " => " << cobjs.size() << endl;
	PRINT_TIME("Cube Merge");
}


// ***************** OVERLAP REMOVAL ****************


unsigned const NDIV(16);


class overlap_remover {

	unsigned inbin[3][2];
	vector<coll_obj> &cobjs;

	unsigned get_bin(float const d, float const b[2]) {

		assert(d >= b[0] && d <= b[1]);
		return min(NDIV-1, unsigned(NDIV*((d - b[0])/(b[1] - b[0])))); // handle edge case
	}

	void get_bin_range(float const d[3][2], float const bb[3][2]) {

		for (unsigned i = 0; i < 3; ++i) {
			for (unsigned j = 0; j < 2; ++j) {
				inbin[i][j] = get_bin(d[i][j], bb[i]);
			}
		}
	}

	void add_to_bins(coll_obj const &cobj, vector<unsigned> bins[NDIV][NDIV][NDIV], float const bb[3][2], unsigned index) {

		get_bin_range(cobj.d, bb);

		for (unsigned b0 = inbin[0][0]; b0 <= inbin[0][1]; ++b0) {
			for (unsigned b1 = inbin[1][0]; b1 <= inbin[1][1]; ++b1) {
				for (unsigned b2 = inbin[2][0]; b2 <= inbin[2][1]; ++b2) {
					bins[b0][b1][b2].push_back(index);
				}
			}
		}
	}

public:
	overlap_remover(vector<coll_obj> &cobjs_) : cobjs(cobjs_) {}

	void remove_overlaps() {

		unsigned overlaps(0);
		vector<coll_obj> new_cobjs;
		vector<unsigned> bins[NDIV][NDIV][NDIV];
		set<unsigned> cids; // for uniquing
		unsigned const nc(cobjs.size());
		float bb[3][2];

		for (unsigned i = 0; i < nc; ++i) { // calculate bbox
			if (cobjs[i].type != COLL_CUBE) continue;

			for (unsigned d = 0; d < 3; ++d) {
				bb[d][0] = ((i == 0) ? cobjs[i].d[d][0] : min(bb[d][0], cobjs[i].d[d][0]));
				bb[d][1] = ((i == 0) ? cobjs[i].d[d][1] : max(bb[d][1], cobjs[i].d[d][1]));
			}
		}
		for (unsigned i = 0; i < nc; ++i) { // fill the bins
			if (cobjs[i].type != COLL_CUBE) continue;
			add_to_bins(cobjs[i], bins, bb, i);
		}
		for (unsigned i = 0; i < cobjs.size(); ++i) {
			if (cobjs[i].type != COLL_CUBE) continue;
			csg_cube cube(cobjs[i]);
			if (cube.is_zero_area()) continue;
			bool const neg(cobjs[i].status == COLL_NEGATIVE);
			get_bin_range(cobjs[i].d, bb);
			cids.clear();

			for (unsigned b0 = inbin[0][0]; b0 <= inbin[0][1]; ++b0) {
				for (unsigned b1 = inbin[1][0]; b1 <= inbin[1][1]; ++b1) {
					for (unsigned b2 = inbin[2][0]; b2 <= inbin[2][1]; ++b2) {
						vector<unsigned> const &v(bins[b0][b1][b2]);

						for (vector<unsigned>::const_iterator it = v.begin(); it != v.end(); ++it) {
							unsigned const j(*it);
							if (cobjs[j].type != COLL_CUBE)               continue; // skip invalid cubes
							if (j == i || cobjs[j].id < cobjs[i].id)      continue; // enforce ordering
							if (neg ^ (cobjs[j].status == COLL_NEGATIVE)) continue; // sign must be the same
							cids.insert(j);
						}
					}
				}
			}
			for (set<unsigned>::const_iterator it = cids.begin(); it != cids.end(); ++it) {
				unsigned const j(*it);
				assert(j < cobjs.size());

				if (cube.subtract_from_cube(new_cobjs, cobjs[j])) {
					if (!new_cobjs.empty()) {
						cobjs[j] = new_cobjs.back();
						new_cobjs.pop_back();
					}
					else {
						cobjs[j].type = COLL_INVALID; // remove old coll obj
						++overlaps;
					}
				}
				for (unsigned i = 0; i < new_cobjs.size(); ++i) { // add in new fragments
					add_to_bins(new_cobjs[i], bins, bb, cobjs.size());
					cobjs.push_back(new_cobjs[i]);
				}
				new_cobjs.clear();
			}
		} // for i
		if (overlaps > 0) remove_invalid_cobjs(cobjs);
	} // remove_overlaps()
};


void remove_overlapping_cubes(vector<coll_obj> &cobjs) { // objects specified later are the ones that are split/removed

	if (!UNOVERLAP_COBJS || cobjs.empty()) return;
	unsigned const ncobjs(cobjs.size());
	RESET_TIME;
	overlap_remover or(cobjs);
	or.remove_overlaps();
	cout << ncobjs << " => " << cobjs.size() << endl;
	PRINT_TIME("Cube Overlap Removal");
}


// **********************************************


bool subtract_cobj(vector<coll_obj> &new_cobjs, csg_cube const &cube, coll_obj &cobj) {

	bool removed(0);

	if (cobj.type == COLL_CUBE) {
		removed = cube.subtract_from_cube(new_cobjs, cobj);
		if (!removed) cube.unset_adjacent_edge_flags(cobj); // check adjacency and possibly remove some edge flags

		for (unsigned i = 0; i < new_cobjs.size(); ++i) {
			cube.unset_adjacent_edge_flags(new_cobjs[i]); // is this necessary?
		}
	}
	else if (cobj.is_cylinder()) {
		removed = cube.subtract_from_cylinder(new_cobjs, cobj);
	}
	return removed;
}


void process_negative_shapes(vector<coll_obj> &cobjs) { // negtive shapes should be non-overlapping

	RESET_TIME;
	unsigned const ncobj(cobjs.size());
	unsigned neg(0);
	vector<coll_obj> new_cobjs;

	for (unsigned i = 0; i < ncobj; ++i) { // find a negative cobj
		if (cobjs[i].status != COLL_NEGATIVE) continue;

		if (cobjs[i].type != COLL_CUBE) {
			cout << "Only negative cubes are supported." << endl;
			exit(1);
		}
		unsigned ncobjs(cobjs.size()); // so as not to retest newly created subcubes
		csg_cube cube(cobjs[i]); // the negative cube
		if (cube.is_zero_area()) continue;

		for (unsigned j = 0; j < ncobjs; ++j) { // find a positive cobj
			if (j != i && cobjs[j].status != COLL_NEGATIVE) {
				if (ONLY_SUB_PREV_NEG && cobjs[i].id < cobjs[j].id) continue; // positive cobj after negative cobj

				if (subtract_cobj(new_cobjs, cube, cobjs[j])) {
					if (!new_cobjs.empty()) { // coll cube can be reused
						cobjs[j] = new_cobjs.back();
						new_cobjs.pop_back();
					}
					else {
						cobjs[j].type = COLL_INVALID; // remove old coll obj
					}
					copy(new_cobjs.begin(), new_cobjs.end(), back_inserter(cobjs)); // add in new fragments
					new_cobjs.clear();
				}
			}
		}
		cobjs[i].type = COLL_INVALID; // remove the negative cube since it is no longer needed
		++neg;
	}
	if (neg > 0) remove_invalid_cobjs(cobjs);
	cout << ncobj << " => " << cobjs.size() << endl;
	PRINT_TIME("Negative Shape Processing");
}


void add_portal(coll_obj const &c) {

	if (c.type == COLL_POLYGON) {
		assert(c.npoints == 3 || c.npoints == 4);
		portal p;

		for (int i = 0; i < c.npoints; ++i) {
			p.pts[i] = c.points[i]; // ignore thickness - use base polygon only
		}
		if (c.npoints == 3) p.pts[3] = p.pts[2]; // duplicate the last point
		portals.push_back(p);
	}
	else if (c.type == COLL_CUBE) {
		portal p;
		float max_area(0.0);
		
		for (unsigned i = 0; i < 6; ++i) { // choose enabled side with max area
			unsigned const dim(i>>1), dir(i&1), d0((dim+1)%3), d1((dim+2)%3);
			if (c.cp.surfs & EFLAGS[dim][dir]) continue; // disabled side
			float const area(fabs(c.d[d0][1] - c.d[d0][0])*fabs(c.d[d1][1] - c.d[d1][0]));

			if (area > max_area) {
				max_area = area;
				point pos;
				pos[dim] = c.d[dim][dir];

				for (unsigned n = 0; n < 4; ++n) {
					pos[d0] = c.d[d0][n<2];
					pos[d1] = c.d[d1][(n&1)^(n<2)];
					p.pts[n] = pos;
				}
			}
		}
		if (max_area > 0.0) portals.push_back(p);
	}
	// else other types are not supported yet (cylinder, sphere)
}


unsigned subtract_cube(vector<coll_obj> &cobjs, vector<color_tid_vol> &cts, vector3d &cdir,
					   float x1, float x2, float y1, float y2, float z1, float z2, int min_destroy)
{
	//RESET_TIME;
	if (destroy_thresh >= EXPLODEABLE) return 0;
	csg_cube const cube(x1, x2, y1, y2, z1, z2);
	point center(cube.get_cube_center());
	if (cube.is_zero_area()) return 0;
	float const sub_volume(cube.get_volume());
	vector<int> indices, to_remove;
	vector<coll_obj> new_cobjs;
	vector<int> cvals;
	cdir = zero_vector;
	unsigned const cobjs_size(cobjs.size());
	float const maxlen(cube.max_len());
	bool const is_small(maxlen < HALF_DXY);
	unsigned ncobjs, last_cobj(0);

	if (is_small) { // not much faster
		int const xpos(get_xpos(center.x)), ypos(get_ypos(center.y));
		if (point_outside_mesh(xpos, ypos)) return 0;
		cvals  = v_collision_matrix[ypos][xpos].cvals; // make a copy because cvals can change during iteration
		ncobjs = cvals.size(); // so as not to retest newly created subcubes
	}
	else {
		ncobjs = cobjs_size; // so as not to retest newly created subcubes
	}
	for (unsigned k = 0; k < ncobjs; ++k) {
		unsigned const i(is_small ? cvals[k] : k);
		assert((size_t)i < cobjs_size);
		if (cobjs[i].status != COLL_STATIC || !cobjs[i].fixed) continue;
		bool const is_cylinder(cobjs[i].is_cylinder()), is_cube(cobjs[i].type == COLL_CUBE), csg_obj(is_cube || is_cylinder);
		int const D(cobjs[i].destroy);
		if (D <= max(destroy_thresh, (min_destroy-1))) continue;
		bool const shatter(D >= SHATTERABLE);
		if (!shatter && !csg_obj)         continue;
		csg_cube const cube2(cobjs[i], !csg_obj);
		if (!cube2.intersects(cube, 0.0)) continue; // no intersection
		//if (is_cube && !cube2.contains_pt(cube.get_cube_center())) {} // check for non-destroyable cobj between center and cube2?
		float volume(cobjs[i].volume);
		bool no_new_cubes(shatter || volume < TOLERANCE);

		if (!csg_obj || subtract_cobj(new_cobjs, cube, cobjs[i]) || (shatter && is_cylinder)) {
			if (no_new_cubes) new_cobjs.clear(); // completely destroyed
			if (is_cube)      cdir += cube2.closest_side_dir(center); // inexact
			if (D == SHATTER_TO_PORTAL) add_portal(cobjs[i]);
			indices.clear();

			for (unsigned j = 0; j < new_cobjs.size(); ++j) { // new objects
				//test_for_falling_cobj(new_cobjs[j], i);
				int const index(new_cobjs[j].add_coll_cobj()); // not sorted by alpha
				assert((size_t)index < cobjs.size());
				indices.push_back(index);
				volume -= cobjs[index].volume;
			}
			assert(volume >= -TOLERANCE); // usually > 0.0
			cts.push_back(color_tid_vol(cobjs[i], volume));
			cobjs[i].clear_internal_data(cobjs, indices, i);
			to_remove.push_back(i);
		}
		new_cobjs.clear();
	} // for k
	if (!to_remove.empty()) {
		//calc_visibility(SUN_SHADOW | MOON_SHADOW); // *** FIXME: what about updating (removing) mesh shadows? ***

		// FIXME: update cobj connectivity and make unconnected cobjs fall

		for (unsigned i = 0; i < to_remove.size(); ++i) {
			remove_coll_object(to_remove[i]); // remove old collision object
		}
		cdir.normalize();
	}
	//PRINT_TIME("Subtract Cube");
	return to_remove.size();
}


bool coll_obj::subdiv_fixed_cube(vector<coll_obj> &cobjs) {

	assert(type == COLL_CUBE);
	float const abs_dmax(get_cube_dmax());
	int maxdim(0);
	float dmax(0.0);

	for (unsigned i = 0; i < 3; ++i) {
		float const len(fabs(d[i][1] - d[i][0]));

		if (i == 0 || len > dmax) {
			maxdim = i;
			dmax   = len;
		}
	}
	if (dmax > abs_dmax) { // large cube - split it
		unsigned char const surfs(cp.surfs);
		unsigned const ndiv(max((unsigned)2, unsigned(dmax/abs_dmax + 0.5)));
		float const d0(d[maxdim][0]), d1(d[maxdim][1]);

		for (unsigned i = 0; i < ndiv; ++i) {
			for (unsigned j = 0; j < 2; ++j) {
				d[maxdim][j] = ((i+j == ndiv) ? d1 : (d0 + ((i+j)*dmax)/ndiv)); // have to be exact to avoid rounding errors
			}
			if (i != 0)      cp.surfs |= EFLAGS[maxdim][0]; // remove interior edges
			if (i != ndiv-1) cp.surfs |= EFLAGS[maxdim][1];
			id       = cobjs.size();
			cobjs.push_back(*this);
			cp.surfs = surfs; // restore edge flags
		}
		return 1;
	}
	return 0;
}


// 0: no intersection, 1: intersection, 2: maybe intersection (incomplete)
// 15 total: 7 complete, 5 partial, 3 unwritten
int coll_obj::intersects_cobj(coll_obj const &c, float toler) const {

	if (c.type < type) return c.intersects_cobj(*this, toler); // swap arguments
	if (!intersects(c, toler)) return 0; // cube-cube intersection

	// c.type >= type
	switch (type) {
	case COLL_CUBE:
		switch (c.type) {
		case COLL_CUBE:
			return 1; // as simple as that
		case COLL_CYLINDER:
			return circle_rect_intersect(c.points[0], c.radius, *this);
		case COLL_SPHERE:
			return sphere_cube_intersect(c.points[0], c.radius, *this);
		case COLL_CYLINDER_ROT:
			if (check_line_clip(c.points[0], c.points[1], d)) return 1; // definite intersection
			return 2; // FIXME
		case COLL_POLYGON:
			for (int i = 0; i < c.npoints; ++i) {
				if (check_line_clip(c.points[i], c.points[(i+1)%c.npoints], d)) return 1; // definite intersection
			}
			// check cube edges for intersection with polygon
			return 2; // FIXME
		default: assert(0);
		}
		break;

	case COLL_CYLINDER:
		switch (c.type) {
		case COLL_CYLINDER:
			return dist_xy_less_than(points[0], c.points[0], (c.radius+radius));
		case COLL_SPHERE:
			return dist_xy_less_than(points[0], c.points[0], (c.radius+radius)); // FIXME: inexact (return 2?)
		case COLL_CYLINDER_ROT:
			if (line_line_dist(points[0], points[1], c.points[0], c.points[1]) > (radius + max(c.radius, c.radius2))) return 0;
			return 2; // FIXME
		case COLL_POLYGON:
			// could use line_intersect_cylinder() for each polygon edge
			return 2; // FIXME
		default: assert(0);
		}
		break;

	case COLL_SPHERE:
		switch (c.type) {
		case COLL_SPHERE:
			return dist_less_than(points[0], c.points[0], (c.radius+radius));
		case COLL_CYLINDER_ROT:
			return sphere_intersect_cylinder(points[0], radius, c.points[0], c.points[1], c.radius, c.radius2);
		case COLL_POLYGON:
			return sphere_ext_poly_intersect(c.points, c.npoints, c.norm, points[0], radius, c.thickness, MIN_POLY_THICK2);
		default: assert(0);
		}
		break;

	case COLL_CYLINDER_ROT:
		switch (c.type) {
		case COLL_CYLINDER_ROT:
			if (line_line_dist(points[0], points[1], c.points[0], c.points[1]) > (max(radius, radius2) + max(c.radius, c.radius2))) return 0;
			return 2; // FIXME
		case COLL_POLYGON:
			// could use line_intersect_cylinder() for each polygon edge
			return 2; // FIXME
		default: assert(0);
		}
		break;

	case COLL_POLYGON:
		switch (c.type) {
		case COLL_POLYGON:
			return 2; // FIXME - need to deal with thickness as well
		default: assert(0);
		}
		break;

	default:
		assert(0);
	}
	return 0;
}


unsigned get_closest_val_index(float val, vector<double> const &sval) {

	for (unsigned i = 0; i < sval.size(); ++i) { // inefficient, assumes sval is small
		if (fabs(val - sval[i]) < TOLER) return i;
	}
	assert(0);
	return 0;
}


void subdiv_cubes(vector<coll_obj> &cobjs) { // split large/high aspect ratio cubes into smaller cubes

	RESET_TIME;
	unsigned size(cobjs.size()), num_remove(0);
	vector<coll_obj> cobjs2;

	// split T-junctions of cubes in the same group
	if (REMOVE_T_JUNCTIONS) {
		map<int, vector<unsigned> > id_map; // id to cobj indices map

		for (unsigned i = 0; i < size; ++i) {
			if (cobjs[i].type == COLL_INVALID || cobjs[i].type != COLL_CUBE)   continue;
			if (REMOVE_T_JUNCTIONS == 1 && cobjs[i].counter != OBJ_CNT_REM_TJ) continue;
			id_map[cobjs[i].id].push_back(i);
		}
		for (map<int, vector<unsigned> >::const_iterator i = id_map.begin(); i != id_map.end(); ++i) {
			vector<unsigned> const &v(i->second);
			if (v.size() == 1) continue; // nothing to do
			set   <double> splits[3]; // x, y, z
			vector<double> svals [3]; // x, y, z

			for (unsigned j = 0; j < v.size(); ++j) {
				coll_obj const &c(cobjs[v[j]]);

				for (unsigned d = 0; d < 3; ++d) {
					for (unsigned e = 0; e < 2; ++e) {
						splits[d].insert(c.d[d][e]);
					}
				}
			}
			for (unsigned d = 0; d < 3; ++d) {
				for (set<double>::const_iterator s = splits[d].begin(); s != splits[d].end(); ++s) {
					if (svals[d].empty() || (*s - svals[d].back()) > TOLER) svals[d].push_back(*s); // skip elements that are near equal
				}
				assert(svals[d].size() > 1);
			}
			for (unsigned j = 0; j < v.size(); ++j) {
				coll_obj const &c(cobjs[v[j]]);
				unsigned bounds[3][2], tot_parts(1);

				for (unsigned d = 0; d < 3; ++d) {
					for (unsigned e = 0; e < 2; ++e) {
						bounds[d][e] = get_closest_val_index(c.d[d][e], svals[d]);
					}
					assert(bounds[d][0] < bounds[d][1] && bounds[d][1] < svals[d].size());
					tot_parts *= (bounds[d][1] - bounds[d][0]);
				}
				assert(tot_parts > 0);
				if (tot_parts == 1) continue; // no splits required
				
				for (unsigned x = bounds[0][0]; x < bounds[0][1]; ++x) {
					for (unsigned y = bounds[1][0]; y < bounds[1][1]; ++y) {
						for (unsigned z = bounds[2][0]; z < bounds[2][1]; ++z) {
							unsigned const xyz[3] = {x, y, z};
							cobjs.push_back(cobjs[v[j]]);

							for (unsigned d = 0; d < 3; ++d) {
								assert(xyz[d]+1 < svals[d].size());

								for (unsigned e = 0; e < 2; ++e) {
									cobjs.back().d[d][e] = svals[d][xyz[d]+e];
								}
							}
						}
					}
				}
				cobjs[v[j]].type = COLL_INVALID;
				++num_remove;
			} // for j
		} // for i
		cout << size << " => " << (cobjs.size() - num_remove) << endl;
		size = cobjs.size();
	}

	// split large cubes
	for (unsigned i = 0; i < size; ++i) {
		if (cobjs[i].type == COLL_INVALID) continue; // skip = remove it

		if (cobjs[i].type != COLL_CUBE || cobjs[i].platform_id >= 0 || !cobjs[i].subdiv_fixed_cube(cobjs2)) {
			cobjs2.push_back(cobjs[i]); // skip platforms
		}
	}
	swap(cobjs, cobjs2);
	cout << (size - num_remove) << " => " << cobjs.size() << endl;
	PRINT_TIME("Subdiv Cubes");
}


bool comp_cobjs_by_draw_params(coll_obj const &a, coll_obj const &b) {
	if (a.cp.tid   < b.cp.tid)   return 1;
	if (b.cp.tid   < a.cp.tid)   return 0;
	if (a.group_id < b.group_id) return 1;
	if (b.group_id < a.group_id) return 0;
	return (get_max_dim(a.norm) < get_max_dim(b.norm));
}

void sort_cobjs_for_rendering(vector<coll_obj> &cobjs) {
	sort(cobjs.begin(), cobjs.end(), comp_cobjs_by_draw_params);
}


color_tid_vol::color_tid_vol(coll_obj const &cobj, float volume_)
	: tid(cobj.cp.tid), destroy(cobj.destroy), draw(cobj.cp.draw), volume(volume_), color(cobj.cp.color)
{
	copy_from(cobj);
}



