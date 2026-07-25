# tests/ — C++ test targets (M0.2 adds the first unit-test executable).
add_executable(chronos_unit_tests
  unit/adapter_sdk_test.cpp
  unit/bybit_websocket_test.cpp
  unit/source_capture_test.cpp
  unit/capture_dataset_test.cpp
  unit/reconnect_test.cpp
  unit/reference_data_test.cpp
  unit/book_normalizer_test.cpp
  unit/trade_normalizer_test.cpp
  unit/replay_provider_test.cpp
  unit/run_input_dispatcher_test.cpp
  unit/strategy_sdk_test.cpp
  unit/imbalance_threshold_strategy_test.cpp
  unit/feature_runtime_test.cpp
  unit/l2_book_test.cpp
  unit/listing_aux_state_test.cpp
  unit/listing_view_publisher_test.cpp
  replay/normalized_stream_golden_test.cpp
  unit/boundary_test.cpp
  unit/event_envelope_test.cpp
  unit/fixed_point_test.cpp
  unit/state_lineage_test.cpp
  unit/serialization_test.cpp
  unit/value_objects_test.cpp
  unit/version_test.cpp)

target_include_directories(chronos_unit_tests
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)

target_compile_definitions(chronos_unit_tests
  PRIVATE CHRONOS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

target_link_libraries(chronos_unit_tests
  PRIVATE chronos_boundary chronos_market_data_normalization chronos_options
          chronos_reference_strategies chronos_replay_providers
          chronos_warnings)

add_test(NAME chronos_unit_tests COMMAND chronos_unit_tests)
