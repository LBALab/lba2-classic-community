<!-- Written by tests/automation/run.sh. -->

# Control-harness last run

- when: 2026-10-21 14:32 UTC
- commit: `d62b4b11`
- host: Darwin arm64
- result: **91 passed, 1 skipped, 1 failed**
- time: 225.5 s with -j 8 (fixtures add up to 1468.8 s)

A skip means a prerequisite was missing, not that the fixture agreed with the
engine. Only the passes are evidence.

| Fixture | Tier | Result | Seconds |
|---|---|---|---|
| test_action_substep | medium | pass | 9.9 |
| test_askchoice_menus_consume | big | pass | 36.2 |
| test_attack_melee | small | pass | 2.3 |
| test_attack_repress | medium | pass | 7.3 |
| test_autocam_orbit_snap | small | pass | 2.3 |
| test_behaviour_cancels_jump | medium | pass | 5.7 |
| test_blowgun_release_throttle | medium | pass | 13.2 |
| test_camera_axes | medium | pass | 7.8 |
| test_camera_interior | small | pass | 1.5 |
| test_camlib_tsv | small | pass | 0.2 |
| test_camzone_blend | medium | pass | 27.9 |
| test_camzone_hold | medium | pass | 9.7 |
| test_camzone_model | small | pass | 2.9 |
| test_camzone_return | big | pass | 101.8 |
| test_camzone_skip | medium | pass | 5.8 |
| test_classiccam_controls | small | pass | 1.6 |
| test_cli_contract | small | pass | 2.5 |
| test_cli_flag_contract | medium | pass | 9.2 |
| test_cli_override_persistence | small | pass | 2.3 |
| test_combo_baseline | medium | pass | 8.7 |
| test_config_upgrade | small | pass | 1.1 |
| test_config_volumes | small | pass | 0.8 |
| test_console_audio_cvars | small | pass | 1.6 |
| test_console_audio_stereo | small | pass | 0.7 |
| test_console_config_write | small | pass | 1.0 |
| test_demo | medium | pass | 15.7 |
| test_demo_behaviour_menu | big | pass | 66.9 |
| test_dodge_direction | medium | pass | 3.2 |
| test_dump | small | pass | 0.3 |
| test_exec | small | pass | 0.5 |
| test_followcam_ease | big | pass | 46.8 |
| test_followcam_hold | small | pass | 2.4 |
| test_followcam_interrupt | small | pass | 2.2 |
| test_followcam_obstruct | medium | pass | 28.1 |
| test_followcam_recenter | small | pass | 4.0 |
| test_followcam_release | small | pass | 3.2 |
| test_followcam_scene_carry | medium | pass | 13.0 |
| test_followcam_tracking | medium | pass | 4.7 |
| test_followcam_world_anchor | big | pass | 55.3 |
| test_found_obj_held_walk | medium | pass | 22.5 |
| test_getascii_text_input | medium | pass | 17.2 |
| test_icon_blend_camera | medium | pass | 16.6 |
| test_input_combos | medium | pass | 16.2 |
| test_input_hold_throttle | medium | pass | 22.8 |
| test_input_injection | small | pass | 0.9 |
| test_item_use_throttle | small | pass | 0.3 |
| test_jump_hold_repress | small | pass | 3.9 |
| test_jump_takeoff_foot | small | pass | 2.3 |
| test_load | small | pass | 0.4 |
| test_melee_throttle | small | pass | 0.8 |
| test_mouse_capture | small | pass | 2.0 |
| test_move_framerate | small | pass | 3.9 |
| test_move_substep | small | pass | 1.2 |
| test_negative | small | pass | 0.4 |
| test_pad_bindings | medium | pass | 5.5 |
| test_pad_config_columns | medium | pass | 4.0 |
| test_polyrec | small | pass | 0.3 |
| test_profile_bind | small | pass | 1.2 |
| test_projection_corpus | medium | FAIL | 30.5 |
| test_projection_demo | big | skip | 0.1 |
| test_record_analog | medium | pass | 19.9 |
| test_record_input_device | big | pass | 40.3 |
| test_record_replay | big | pass | 212.0 |
| test_release_detection | small | pass | 0.8 |
| test_res_catalog_sweep | medium | pass | 13.8 |
| test_res_switch | small | pass | 1.0 |
| test_res_switch_cfg | small | pass | 1.2 |
| test_res_switch_console | medium | pass | 4.2 |
| test_res_switch_dialog | small | pass | 3.3 |
| test_ring_off_is_gone | big | pass | 178.3 |
| test_ring_watch_not_play | big | pass | 222.6 |
| test_save_auto_name_refresh | big | pass | 45.0 |
| test_screenshot | small | pass | 0.4 |
| test_skip_modals | small | pass | 0.3 |
| test_spell_keys_bypass | small | pass | 7.1 |
| test_tick | small | pass | 1.0 |
| test_ui_dialog | small | pass | 1.8 |
| test_ui_display | small | pass | 2.2 |
| test_ui_found_object | small | pass | 1.6 |
| test_ui_holomap | small | pass | 1.6 |
| test_ui_holoplan | small | pass | 1.6 |
| test_ui_inventory | small | pass | 1.3 |
| test_ui_inventory_wide | small | pass | 1.8 |
| test_ui_menu_main | small | pass | 2.2 |
| test_ui_menu_main_fresh | small | pass | 3.0 |
| test_ui_menu_main_options | medium | pass | 21.1 |
| test_ui_menu_options | small | pass | 2.1 |
| test_ui_menu_options_hd | small | pass | 1.9 |
| test_ui_menu_options_uhd | small | pass | 2.2 |
| test_ui_menu_options_wide | small | pass | 1.5 |
| test_ui_video | small | pass | 1.8 |
| test_user_dirs | small | pass | 0.9 |
| test_walkthrough_opening | medium | pass | 5.7 |
