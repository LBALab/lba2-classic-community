<!-- Written by tests/automation/run.sh. -->

# Control-harness last run

- when: 2026-10-21 15:45 UTC
- commit: `fb5bf4dd`
- host: Darwin arm64
- result: **91 passed, 1 skipped, 1 failed**
- time: 283.9 s with -j 8 (fixtures add up to 1211.7 s)

A skip means a prerequisite was missing, not that the fixture agreed with the
engine. Only the passes are evidence.

| Fixture | Tier | Result | Seconds |
|---|---|---|---|
| test_action_substep | medium | pass | 3.5 |
| test_askchoice_menus_consume | big | pass | 35.9 |
| test_attack_melee | small | pass | 1.1 |
| test_attack_repress | medium | pass | 3.8 |
| test_autocam_orbit_snap | small | pass | 1.1 |
| test_behaviour_cancels_jump | medium | pass | 2.4 |
| test_blowgun_release_throttle | medium | pass | 4.8 |
| test_camera_axes | medium | pass | 2.7 |
| test_camera_interior | small | pass | 1.1 |
| test_camlib_tsv | small | pass | 0.2 |
| test_camzone_blend | medium | pass | 11.0 |
| test_camzone_hold | medium | pass | 4.1 |
| test_camzone_model | small | pass | 1.5 |
| test_camzone_return | big | pass | 42.2 |
| test_camzone_skip | medium | pass | 2.9 |
| test_classiccam_controls | small | pass | 1.0 |
| test_cli_contract | small | pass | 1.8 |
| test_cli_flag_contract | medium | pass | 7.3 |
| test_cli_override_persistence | small | pass | 1.5 |
| test_combo_baseline | medium | pass | 3.7 |
| test_config_upgrade | small | pass | 0.9 |
| test_config_volumes | small | pass | 0.7 |
| test_console_audio_cvars | small | pass | 1.3 |
| test_console_audio_stereo | small | pass | 0.6 |
| test_console_config_write | small | pass | 0.9 |
| test_demo | medium | pass | 6.2 |
| test_demo_behaviour_menu | big | pass | 31.2 |
| test_dodge_direction | medium | pass | 2.6 |
| test_dump | small | pass | 0.3 |
| test_exec | small | pass | 0.5 |
| test_followcam_ease | big | pass | 22.1 |
| test_followcam_hold | small | pass | 1.6 |
| test_followcam_interrupt | small | pass | 1.4 |
| test_followcam_obstruct | medium | pass | 12.9 |
| test_followcam_recenter | small | pass | 2.3 |
| test_followcam_release | small | pass | 2.1 |
| test_followcam_scene_carry | medium | pass | 5.4 |
| test_followcam_tracking | medium | pass | 2.6 |
| test_followcam_world_anchor | big | pass | 27.7 |
| test_found_obj_held_walk | medium | pass | 12.2 |
| test_getascii_text_input | medium | pass | 8.8 |
| test_icon_blend_camera | medium | pass | 11.4 |
| test_input_combos | medium | pass | 9.8 |
| test_input_hold_throttle | medium | pass | 14.2 |
| test_input_injection | small | pass | 1.3 |
| test_item_use_throttle | small | pass | 0.4 |
| test_jump_hold_repress | small | pass | 3.9 |
| test_jump_takeoff_foot | small | pass | 2.6 |
| test_load | small | pass | 0.5 |
| test_melee_throttle | small | pass | 0.8 |
| test_mouse_capture | small | pass | 1.5 |
| test_move_framerate | small | pass | 2.1 |
| test_move_substep | small | pass | 1.5 |
| test_negative | small | pass | 0.6 |
| test_pad_bindings | medium | pass | 4.7 |
| test_pad_config_columns | medium | pass | 3.1 |
| test_polyrec | small | pass | 0.2 |
| test_profile_bind | small | pass | 1.3 |
| test_projection_corpus | medium | FAIL | 38.1 |
| test_projection_demo | big | skip | 0.1 |
| test_record_analog | medium | pass | 13.9 |
| test_record_input_device | big | pass | 40.3 |
| test_record_replay | big | pass | 215.8 |
| test_release_detection | small | pass | 0.8 |
| test_res_catalog_sweep | medium | pass | 14.0 |
| test_res_switch | small | pass | 1.2 |
| test_res_switch_cfg | small | pass | 2.4 |
| test_res_switch_console | medium | pass | 4.9 |
| test_res_switch_dialog | small | pass | 3.6 |
| test_ring_off_is_gone | big | pass | 185.7 |
| test_ring_watch_not_play | big | pass | 225.6 |
| test_save_auto_name_refresh | big | pass | 47.1 |
| test_screenshot | small | pass | 1.0 |
| test_skip_modals | small | pass | 1.1 |
| test_spell_keys_bypass | small | pass | 5.1 |
| test_tick | small | pass | 1.5 |
| test_ui_dialog | small | pass | 2.4 |
| test_ui_display | small | pass | 3.1 |
| test_ui_found_object | small | pass | 2.5 |
| test_ui_holomap | small | pass | 2.6 |
| test_ui_holoplan | small | pass | 3.0 |
| test_ui_inventory | small | pass | 2.3 |
| test_ui_inventory_wide | small | pass | 2.4 |
| test_ui_menu_main | small | pass | 3.0 |
| test_ui_menu_main_fresh | small | pass | 4.1 |
| test_ui_menu_main_options | medium | pass | 23.1 |
| test_ui_menu_options | small | pass | 4.0 |
| test_ui_menu_options_hd | small | pass | 3.0 |
| test_ui_menu_options_uhd | small | pass | 4.1 |
| test_ui_menu_options_wide | small | pass | 2.6 |
| test_ui_video | small | pass | 3.0 |
| test_user_dirs | small | pass | 1.3 |
| test_walkthrough_opening | medium | pass | 5.3 |
