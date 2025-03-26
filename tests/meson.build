run_test = find_program('run-test.sh')

pytest_args = [
  meson.current_source_dir(),
  '--verbose',
  '--log-level=DEBUG',
  '-s',
]

pytest_env = environment()
pytest_env.set('BUILDDIR', meson.project_build_root())

test(
  'xnmp',
  run_test,
  args:pytest_args,
  env: pytest_env,
  timeout: 120,
)