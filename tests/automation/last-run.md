<!-- Written by tests/automation/run.sh. -->

# Control-harness last run

- when: 2026-10-22 07:54 UTC
- commit: `0dec0259` (working tree dirty)
- host: Darwin arm64
- result: **91 passed, 1 skipped, 1 failed**
- time: 253.8 s with -j 8 (fixtures add up to 1287.0 s)

A skip means a prerequisite was missing, not that the fixture agreed with the
engine. Only the passes are evidence.

| Fixture | Tier | Result | Seconds |
|---|---|---|---|
| test_action_substep | medium | pass | 8.7 |
| test_askchoice_menus_consume | big | pass | 37.5 |
| test_attack_melee | small | pass | 1.3 |
| test_attack_repress | medium | pass | 5.9 |
| test_autocam_orbit_snap | small | pass | 1.7 |
| test_behaviour_cancels_jump | medium | pass | 7.1 |
| test_blowgun_release_throttle | medium | pass | 12.3 |
| test_camera_axes | medium | pass | 5.5 |
| test_camera_interior | small | pass | 1.5 |
| test_camlib_tsv | small | pass | 0.2 |
| test_camzone_blend | medium | pass | 28.2 |
| test_camzone_hold | medium | pass | 9.5 |
| test_camzone_model | small | pass | 3.2 |
| test_camzone_return | big | pass | 92.2 |
| test_camzone_skip | medium | pass | 4.6 |
| test_classiccam_controls | small | pass | 1.6 |
| test_cli_contract | small | pass | 2.9 |
| test_cli_flag_contract | medium | pass | 10.2 |
| test_cli_override_persistence | small | pass | 3.6 |
| test_combo_baseline | medium | pass | 8.7 |
| test_config_upgrade | small | pass | 1.3 |
| test_config_volumes | small | pass | 0.8 |
| test_console_audio_cvars | small | pass | 1.5 |
| test_console_audio_stereo | small | pass | 0.8 |
| test_console_config_write | small | pass | 1.6 |
| test_demo | medium | pass | 14.2 |
| test_demo_behaviour_menu | big | pass | 60.5 |
| test_dodge_direction | medium | pass | 4.2 |
| test_dump | small | pass | 0.3 |
| test_exec | small | pass | 0.6 |
| test_followcam_ease | big | pass | 44.0 |
| test_followcam_hold | small | pass | 3.8 |
| test_followcam_interrupt | small | pass | 2.8 |
| test_followcam_obstruct | medium | pass | 27.8 |
| test_followcam_recenter | small | pass | 5.2 |
| test_followcam_release | small | pass | 3.6 |
| test_followcam_scene_carry | medium | pass | 11.6 |
| test_followcam_tracking | medium | pass | 6.6 |
| test_followcam_world_anchor | big | pass | 52.7 |
| test_found_obj_held_walk | medium | pass | 22.5 |
| test_getascii_text_input | medium | pass | 18.6 |
| test_icon_blend_camera | medium | pass | 16.7 |
| test_input_combos | medium | pass | 9.8 |
| test_input_hold_throttle | medium | pass | 23.4 |
| test_input_injection | small | pass | 0.9 |
| test_item_use_throttle | small | pass | 0.4 |
| test_jump_hold_repress | small | pass | 6.2 |
| test_jump_takeoff_foot | small | pass | 3.1 |
| test_load | small | pass | 0.4 |
| test_melee_throttle | small | pass | 0.8 |
| test_mouse_capture | small | pass | 3.1 |
| test_move_framerate | small | pass | 4.8 |
| test_move_substep | small | pass | 1.4 |
| test_negative | small | pass | 0.5 |
| test_pad_bindings | medium | pass | 6.5 |
| test_pad_config_columns | medium | pass | 5.7 |
| test_polyrec | small | pass | 0.3 |
| test_profile_bind | small | pass | 1.2 |
| test_projection_corpus | medium | FAIL | 30.3 |
| test_projection_demo | big | skip | 0.1 |
| test_record_analog | medium | pass | 17.2 |
| test_record_input_device | big | pass | 36.7 |
| test_record_replay | big | pass | 90.5 |
| test_release_detection | small | pass | 0.8 |
| test_res_catalog_sweep | medium | pass | 12.7 |
| test_res_switch | small | pass | 0.9 |
| test_res_switch_cfg | small | pass | 1.1 |
| test_res_switch_console | medium | pass | 5.9 |
| test_res_switch_dialog | small | pass | 4.1 |
| test_ring_off_is_gone | big | pass | 170.4 |
| test_ring_watch_not_play | big | pass | 198.8 |
| test_save_auto_name_refresh | big | pass | 41.0 |
| test_screenshot | small | pass | 0.5 |
| test_skip_modals | small | pass | 0.4 |
| test_spell_keys_bypass | small | pass | 4.1 |
| test_tick | small | pass | 0.7 |
| test_ui_dialog | small | pass | 1.3 |
| test_ui_display | small | pass | 3.7 |
| test_ui_found_object | small | pass | 1.3 |
| test_ui_holomap | small | pass | 1.1 |
| test_ui_holoplan | small | pass | 1.6 |
| test_ui_inventory | small | pass | 1.1 |
| test_ui_inventory_wide | small | pass | 1.2 |
| test_ui_menu_main | small | pass | 3.0 |
| test_ui_menu_main_fresh | small | pass | 4.1 |
| test_ui_menu_main_options | medium | pass | 19.3 |
| test_ui_menu_options | small | pass | 3.0 |
| test_ui_menu_options_hd | small | pass | 1.6 |
| test_ui_menu_options_uhd | small | pass | 2.9 |
| test_ui_menu_options_wide | small | pass | 1.3 |
| test_ui_video | small | pass | 1.9 |
| test_user_dirs | small | pass | 0.9 |
| test_walkthrough_opening | medium | pass | 5.1 |
