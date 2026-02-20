/**
 * Display labels for request_category (snake_case from CSV).
 * Fallback: format unknown keys as title case from snake_case.
 */

export const REQUEST_CATEGORY_LABELS = {
  extra_towels: 'Extra towels',
  room_cleaning_request: 'Room cleaning',
  maintenance_issue_found: 'Maintenance issue',
  luggage_assistance: 'Luggage assistance',
  early_checkin_prep: 'Early check-in prep',
  guest_lockout: 'Guest lockout',
  hvac_issue: 'HVAC issue',
  noise_complaint: 'Noise complaint',
  room_ready: 'Room ready',
  room_service_order: 'Room service order',
  extra_pillows: 'Extra pillows',
  plumbing_issue: 'Plumbing issue',
  supply_restock_needed: 'Supply restock',
  repair_completed: 'Repair completed',
  late_checkout_approval: 'Late checkout approval',
  room_change_request: 'Room change',
  taxi_request: 'Taxi request',
  lost_found_item: 'Lost & found',
  parking_issue: 'Parking issue',
  wifi_issue: 'Wi‑Fi issue',
  tv_not_working: 'TV not working',
  suspicious_activity: 'Suspicious activity',
  vip_arrival: 'VIP arrival',
  room_out_of_order: 'Room out of order',
  minibar_restock: 'Minibar restock',
  elevator_issue: 'Elevator issue',
};

/**
 * @param {string} key - request_category value (e.g. "extra_towels")
 * @returns {string} - Human-readable label; fallback is title case from snake_case
 */
export function getRequestCategoryLabel(key) {
  if (!key || typeof key !== 'string') return '';
  const trimmed = key.trim();
  if (REQUEST_CATEGORY_LABELS[trimmed]) return REQUEST_CATEGORY_LABELS[trimmed];
  return trimmed
    .split('_')
    .map((w) => w.charAt(0).toUpperCase() + w.slice(1).toLowerCase())
    .join(' ');
}

/**
 * Filter out keys that look like locations or noise (e.g. "Room 704", "Conference Room A").
 * Keep only keys that look like category slugs (snake_case, no leading "Room " etc.).
 */
export function isCategoryKey(key) {
  if (!key || typeof key !== 'string') return false;
  const k = key.trim();
  if (!k) return false;
  if (k.startsWith('Room ') && /\d+/.test(k)) return false;
  if (/^Room \d+$/.test(k)) return false;
  if (k.includes(' ') && !k.includes('_')) return false;
  return true;
}
