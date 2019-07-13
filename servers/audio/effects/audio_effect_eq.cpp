/*************************************************************************/
/*  audio_effect_eq.cpp                                                  */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "audio_effect_eq.h"
#include "servers/audio_server.h"

void AudioEffectEQInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {

	int band_count = bands[0].size();
	EQ::BandProcess *proc_l = bands[0].ptrw();
	EQ::BandProcess *proc_r = bands[1].ptrw();
	float *bgain = gains.ptrw();
	for (int i = 0; i < band_count; i++) {
		bgain[i] = Math::db2linear(base->gain[i]);
	}

	for (int i = 0; i < p_frame_count; i++) {

		AudioFrame src = p_src_frames[i];
		AudioFrame dst = AudioFrame(0, 0);

		for (int j = 0; j < band_count; j++) {

			float l = src.l;
			float r = src.r;

			proc_l[j].process_one(l);
			proc_r[j].process_one(r);

			dst.l += l * bgain[j];
			dst.r += r * bgain[j];
		}

		p_dst_frames[i] = dst;
	}
}

Ref<AudioEffectInstance> AudioEffectEQ::instance() {
	Ref<AudioEffectEQInstance> ins;
	ins.instance();
	ins->base = Ref<AudioEffectEQ>(this);
	ins->gains.resize(eq.get_band_count());
	for (int i = 0; i < 2; i++) {
		ins->bands[i].resize(eq.get_band_count());
		for (int j = 0; j < ins->bands[i].size(); j++) {
			ins->bands[i].write[j] = eq.get_band_processor(j);
		}
	}

	return ins;
}

float AudioEffectEQ::get_band_frequency(int p_band) const {
	ERR_FAIL_INDEX_V(p_band, gain.size(), 0);
	return eq.get_band_frequency(p_band);
}

int AudioEffectEQ::get_band_count() const {
	return gain.size();
}

void AudioEffectEQ::set_band_gain_db(int p_band, float p_volume) {
	ERR_FAIL_INDEX(p_band, gain.size());
	gain.write[p_band] = p_volume;
}

float AudioEffectEQ::get_band_gain_db(int p_band) const {
	ERR_FAIL_INDEX_V(p_band, gain.size(), 0);

	return gain[p_band];
}

bool AudioEffectEQ::_set(const StringName &p_name, const Variant &p_value) {

	const Map<StringName, int>::Element *E = prop_band_map.find(p_name);
	if (E) {
		set_band_gain_db(E->get(), p_value);
		return true;
	}

	return false;
}

bool AudioEffectEQ::_get(const StringName &p_name, Variant &r_ret) const {

	const Map<StringName, int>::Element *E = prop_band_map.find(p_name);
	if (E) {
		r_ret = get_band_gain_db(E->get());
		return true;
	}

	return false;
}

void AudioEffectEQ::_get_property_list(List<PropertyInfo> *p_list) const {

	for (int i = 0; i < band_names.size(); i++) {

		p_list->push_back(PropertyInfo(Variant::REAL, band_names[i], PROPERTY_HINT_RANGE, "-60,24,0.1"));
	}
}

void AudioEffectEQ::_bind_methods() {

	ClassDB::bind_method(D_METHOD("get_band_count"), &AudioEffectEQ::get_band_count);
	ClassDB::bind_method(D_METHOD("get_band_frequency", "band_idx"), &AudioEffectEQ::get_band_frequency);
	ClassDB::bind_method(D_METHOD("set_band_gain_db", "band_idx", "volume_db"), &AudioEffectEQ::set_band_gain_db);
	ClassDB::bind_method(D_METHOD("get_band_gain_db", "band_idx"), &AudioEffectEQ::get_band_gain_db);
}

AudioEffectEQ::AudioEffectEQ() {

	eq.set_mix_rate(AudioServer::get_singleton()->get_mix_rate());
}

void AudioEffectEQPreset::_bind_methods() {

	//ClassDB::bind_method(D_METHOD("nonsense", "count"), &AudioEffectEQPreset::set_band_count);
}

AudioEffectEQPreset::AudioEffectEQPreset(EQ::Preset p_preset) {

	eq.set_preset_band_mode(p_preset);
	gain.resize(eq.get_band_count());
	for (int i = 0; i < gain.size(); i++) {
		gain.write[i] = 0.0;
		String name = "band_db/" + itos(eq.get_band_frequency(i)) + "_hz";
		prop_band_map[name] = i;
		band_names.push_back(name);
	}
}

void AudioEffectEQCustom::set_band_count(int p_count) {

	eq.set_band_count(p_count);
	gain.resize(p_count);
	prop_band_map.clear();
	band_names.clear();
	for (int i = 0; i < gain.size(); i++) {
		gain.write[i] = 0.0;
		String name = "band_db/band_" + itos(i);
		prop_band_map[name] = i;
		band_names.push_back(name);
	}
}

void AudioEffectEQCustom::set_band_frequency(int p_band, float p_freq) {
}

bool AudioEffectEQCustom::_set(const StringName &p_name, const Variant &p_value) {

	printf(">> %ls\n", ((String)p_name).c_str());
	if (AudioEffectEQ::_set(p_name, p_value)) return true;

	if (p_name == "band_count") {
		int count = (int)p_value;
		count = (count < 0 ? 0 : (count > 31 ? 31 : count));
		set_band_count(count);
		return true;
	} else {
		String prefix = "band_frequency/band_";
		String name = (String)p_name;
		if (name.begins_with(prefix)) {
			int idx = name.substr(prefix.length()).to_int();
			if (idx >= 0 && idx < 31) {
				if (idx >= eq.get_band_count())
					set_band_count(idx + 1);
				float freq = (float)p_value;
				freq = (freq < 10 ? 10 : (freq > 22050 ? 22050 : freq));
				eq.set_band_frequency(idx, freq);
				return true;
			}
		}
	}

	return false;
}

bool AudioEffectEQCustom::_get(const StringName &p_name, Variant &r_ret) const {

	if (AudioEffectEQ::_get(p_name, r_ret)) return true;

	if (p_name == "band_count") {
		r_ret = gain.size();
		return true;
	} else {
		String prefix = "band_frequency/band_";
		String name = (String)p_name;
		if (name.begins_with(prefix)) {
			int idx = name.substr(prefix.length()).to_int();
			if (idx >= 0 && idx < eq.get_band_count()) {
				r_ret = eq.get_band_frequency(idx);
				return true;
			}
		}
	}

	return false;
}

void AudioEffectEQCustom::_get_property_list(List<PropertyInfo> *p_list) const {

	p_list->push_back(PropertyInfo(Variant::INT, "band_count", PROPERTY_HINT_RANGE, "0,31,1"));

	for (int i = 0; i < gain.size(); i++) {
		p_list->push_back(PropertyInfo(Variant::REAL, "band_frequency/band_" + itos(i), PROPERTY_HINT_EXP_RANGE, "10,22050,1"));
	}
}

void AudioEffectEQCustom::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_band_count", "count"), &AudioEffectEQCustom::set_band_count);
	ClassDB::bind_method(D_METHOD("set_band_frequency", "frequency"), &AudioEffectEQCustom::set_band_frequency);
}

AudioEffectEQCustom::AudioEffectEQCustom() {
}
