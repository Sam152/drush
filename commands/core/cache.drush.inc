<?php

/**
 * Implementation of hook_drush_command().
 */
function cache_drush_command() {
  $items = array();

  // We specify command callbacks here because the defaults would collide with
  // the drush cache api functions.
  $items['cache-get'] = array(
    'description' => 'Fetch a cached object and display it.',
    'examples' => array(
      'drush cache-get schema' => 'Display the data for the cache id "schema" from the "cache" bin.',
      'drush cache-get update_available_releases update' => 'Display the data for the cache id "update_available_releases" from the "update" bin.',
    ),
    'arguments' => array(
      'cid' => 'The id of the object to fetch.',
      'bin' => 'Optional. The cache bin to fetch from.',
    ),
    'required-arguments' => 1,
    'callback' => 'drush_cache_command_get',
    'outputformat' => array(
      'default' => 'print-r',
      'pipe-format' => 'var_export',
      'output-data-type' => TRUE,
    ),
    'aliases' => array('cg'),
  );
  $items['cache-clear'] = array(
    'bootstrap' => DRUSH_BOOTSTRAP_MAX,
    'description' => 'Clear a specific cache, or all drupal caches.',
    'arguments' => array(
      'type' => 'The particular cache to clear. Omit this argument to choose from available caches.',
    ),
    'callback' => 'drush_cache_command_clear',
    'aliases' => array('cc'),
  );
  $items['cache-set'] = array(
    'description' => 'Cache an object expressed in JSON or var_export() format.',
    'arguments' => array(
      'cid' => 'The id of the object to set.',
      'data' => 'The object to set in the cache. Use \'-\' to read the object from STDIN.',
      'bin' => 'Optional. The cache bin to store the object in.',
      'expire' => 'Optional. CACHE_PERMANENT, CACHE_TEMPORARY, or a Unix timestamp.',
    ),
    'required-arguments' => 2,
    'options' => array(
      // Note that this is not an outputformat option.
      'format' => 'Format to parse the object. Use "string" for string (default), and "json" for JSON.',
      'cache-get' => 'If the object is the result a previous fetch from the cache, only store the value in the "data" property of the object in the cache.',
    ),
    'callback' => 'drush_cache_command_set',
    'aliases' => array('cs'),
  );
  $items['cache-rebuild'] = array(
    'description' => 'Rebuild a Drupal 8 site and clear all its caches.',
    // rebuild.php only bootstraps to DRUPAL_BOOTSTRAP_CONFIGURATION, so we'll
    // do the same
    'options' => array(),
    'arguments' => array(),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_CONFIGURATION,
    'core' => array('8+'),
    'aliases' => array('cr', 'rebuild'),
  );

  return $items;
}

/**
 * Command argument complete callback.
 *
 * @return
 *   Array of clear types.
 */
function cache_cache_command_clear_complete() {
  return array('values' => array_keys(drush_cache_clear_types(TRUE)));
}

function drush_cache_clear_pre_validate($type = NULL) {
  $types = drush_cache_clear_types(drush_has_boostrapped(DRUSH_BOOTSTRAP_DRUPAL_FULL));
  // Check if the provided type ($type) is a valid cache type.
  if ($type && !key_exists($type, $types)) {
    if ($type === 'all' && drush_drupal_major_version() >= 8) {
      return drush_set_error(dt('`cache-clear all` is deprecated for Drupal 8 and later. Please use the `cache-rebuild` command instead.'));
    }
    // If we haven't done a full bootstrap, provide a more
    // specific message with instructions to the user on
    // bootstrapping a Drupal site for more options.
    if (!drush_has_boostrapped(DRUSH_BOOTSTRAP_DRUPAL_FULL)) {
      $all_types = drush_cache_clear_types(TRUE);
      if (key_exists($type, $all_types)) {
        return drush_set_error(dt("'!type' cache requires a working Drupal site to operate on. Use the --root and --uri options, or a site @alias, or cd to a directory containing a Drupal settings.php file.", array('!type' => $type)));
      }
      else {
        return drush_set_error(dt("'!type' cache is not a valid cache type. There may be more cache types available if you select a working Drupal site.", array('!type' => $type)));
      }
    }
    return drush_set_error(dt("'!type' cache is not a valid cache type.", array('!type' => $type)));
  }
}

/**
 * Command callback for drush cache-clear.
 */
function drush_cache_command_clear($type = NULL) {
  $types = drush_cache_clear_types(drush_has_boostrapped(DRUSH_BOOTSTRAP_DRUPAL_FULL));

  if ($type) {
    drush_op($types[$type]);
  }
  else {
    // Don't offer 'all' unless Drush has bootstrapped the Drupal site
    if (!drush_has_boostrapped(DRUSH_BOOTSTRAP_DRUPAL_FULL)) {
      unset($types['all']);
    }
    $type = drush_choice($types, 'Enter a number to choose which cache to clear.', '!key');
    if (empty($type)) {
      return drush_user_abort();
    }
  }
  // Do it.
  drush_op($types[$type]);
  if ($type == 'all' && !drush_has_boostrapped(DRUSH_BOOTSTRAP_DRUPAL_FULL)) {
    drush_log(dt("No Drupal site found, only 'drush' cache was cleared."), 'warning');
  }
  drush_log(dt("'!name' cache was cleared.", array('!name' => $type)), 'success');
}

/**
 * Print an object returned from the cache.
 *
 * @param $cid
 *   The cache ID of the object to fetch.
 * @param $bin
 *   A specific bin to fetch from.
 */
function drush_cache_command_get($cid = NULL, $bin = 'cache') {
  if (drush_drupal_major_version() >=8) {
    $result = cache($bin)->get($cid);
  }
  else {
    $result = cache_get($cid, $bin);
  }
  if (empty($result)) {
    return drush_set_error('DRUSH_CACHE_OBJECT_NOT_FOUND', dt('The !cid object in the !bin cache bin was not found.', array('!cid' => $cid, '!bin' => $bin)));
  }
  return $result;
}

/**
 * Set an object in the cache.
 *
 * @param $cid
 *   The cache ID of the object to fetch.
 * @param $data
 *   The data to save to the cache, or '-' to read from STDIN.
 * @param $bin
 *   A specific bin to fetch from.
 * @param $expire
 *   The expiry timestamp for the cached object.
 * @param $tags
 *   Cache tags for the cached object.
 */
function drush_cache_command_set($cid = NULL, $data = '', $bin = 'cache', $expire = CACHE_PERMANENT, $tags = NULL) {
  // In addition to prepare, this also validates. Can't easily be in own validate callback as
  // reading once from STDIN empties it.
  $data = drush_cache_set_prepare_data($data);
  if ($data === FALSE && drush_get_error()) {
    // An error was logged above.
    return;
  }

  // Convert the "expire" argument to a valid value for Drupal's cache_set().
  if ($expire == 'CACHE_TEMPORARY') {
    $expire = CACHE_TEMPORARY;
  }
  if ($expire == 'CACHE_PERMANENT') {
    $expire = CACHE_PERMANENT;
  }

  cache_set($cid, $data, $bin, $expire);
}

function drush_cache_set_prepare_data($data) {
  if ($data == '-') {
    $data = file_get_contents("php://stdin");
  }

  // Now, we parse the object.
  switch (drush_get_option('format', 'string')) {
    case 'json':
      $data = drush_json_decode($data);
      break;
  }

  if (drush_get_option('cache-get')) {
    // $data might be an object.
    if (is_object($data) && $data->data) {
      $data = $data->data;
    }
    // But $data returned from `drush cache-get --format=json` will be an array.
    elseif (is_array($data) && isset($data['data'])) {
      $data = $data['data'];
    }
    else {
      // If $data is neither object nor array and cache-get was specified, then
      // there is a problem.
      return drush_set_error('CACHE_INVALID_FORMAT', dt("'cache-get' was specified as an option, but the data is neither an object or an array."));
    }
  }

  return $data;
}

/**
 * All types of caches available for clearing. Contrib commands can alter in their own.
 */
function drush_cache_clear_types($include_bootstraped_types = FALSE) {
  $types = array(
    'drush' => 'drush_cache_clear_drush',
  );
  if ($include_bootstraped_types) {
    $types += array(
      'theme-registry' => 'drush_cache_clear_theme_registry',
      'menu' => 'menu_rebuild',
      'css-js' => 'drush_cache_clear_css_js',
      'block' => 'drush_cache_clear_block',
      'module-list' => 'drush_get_modules',
      'theme-list' => 'drush_get_themes',
    );
  }

  $drupal_version = drush_drupal_major_version();

  // D8 renamed menu_rebuild() (https://drupal.org/node/1561492).
  if ($drupal_version >= 8) {
    $types['menu'] = 'menu_router_rebuild';
  }

  if ($drupal_version >= 7) {
    $types['registry'] = 'registry_update';
  }
  elseif ($drupal_version == 6 && function_exists('module_exists') && module_exists('autoload')) {
    // TODO: move this to autoload module.
    $types['registry'] = 'autoload_registry_update';
  }

  // Don't allow "all" cache type on D8 and later - we want users to use the
  // "cache-rebuild" command instead.
  if ($drupal_version <= 7) {
    $types['all'] = 'drush_cache_clear_both';
  }

  // Include the appropriate environment engine, so callbacks can use core
  // version specific cache clearing functions directly.
  drush_include_engine('drupal', 'environment');

  // Command files may customize $types as desired.
  drush_command_invoke_all_ref('drush_cache_clear', $types);

  return $types;
}

function drush_cache_clear_theme_registry() {
  if (drush_drupal_major_version() >= 7) {
    drupal_theme_rebuild();
  }
  else {
    cache_clear_all('theme_registry', 'cache', TRUE);
  }
}

function drush_cache_clear_css_js() {
  _drupal_flush_css_js();
  drupal_clear_css_cache();
  drupal_clear_js_cache();
}

/**
 * Clear the cache of the block output.
 */
function drush_cache_clear_block() {
  cache_clear_all(NULL, 'cache_block');
}

/**
 * Clear caches internal to drush core.
 */
function drush_cache_clear_drush() {
  drush_cache_clear_all(NULL, 'default'); // commandfiles, etc.
  drush_cache_clear_all(NULL, 'complete'); // completion
}

/**
 * Clear caches internal to Drush core and Drupal.
 */
function drush_cache_clear_both() {
  // We want Drupal 8 users to use the `drush cache-rebuild` command instead.
  if (drush_drupal_major_version() >= 8) {
    return drush_set_error('DRUSH_CC_ALL_DEPRECATED', dt('`clear-cache all` is deprecated for Drupal 8 and later. Please use the `cache-rebuild` command instead.'));
  }
  drush_cache_clear_drush();
  if (drush_has_boostrapped(DRUSH_BOOTSTRAP_DRUPAL_FULL)) {
    drupal_flush_all_caches();
  }
}

/**
 * Rebuild a Drupal 8 site.
 */
function drush_cache_rebuild() {
  require_once DRUPAL_ROOT . '/core/includes/utility.inc';
  // drupal_rebuild() calls drupal_flush_all_caches() itself, so we don't do it
  // manually.
  drupal_rebuild();
  // As this command replaces `drush cache-clear all` for Drupal 8 users, clear
  // the Drush cache as well, for consistency with that behavior.
  drush_cache_clear_drush();
}
