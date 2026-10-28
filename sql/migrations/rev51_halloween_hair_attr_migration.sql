-- =============================================================================
-- Metin2 MT2009 - Post-Rev 51 Legacy Halloween Hair Costume Attribute Migration
-- Target Engine: MariaDB 12.3 LTS (STRICT_TRANS_TABLES, NO_ZERO_DATE)
-- Target Vnums:  74020, 74270, 74520, 74770, 75020, 75220, 75420, 75620
--                (ITEM_COSTUME = 28, COSTUME_HAIR = 1, LIMIT_REAL_TIME_START_FIRST_USE)
--
-- PURPOSE:
--   Prior to commit c186fed1a6374414a94c206a85a30128c0a4966d,
--   `event_halloween_hair.quest` passed `questlib.lua` `apply.*` indices
--   directly to `item.set_value(0, option.apply, option.value)`, storing raw
--   apply indices (59, 60, 61, 62, 63, 69, 70) in `item.attrtype0` instead of
--   `EPointTypes` (54, 55, 56, 57, 53, 119, 120).
--
--   Because raw apply numbers (59..63, 69, 70) collide with legitimate
--   `EPointTypes` (`POINT_RESIST_WARRIOR..SHAMAN`, `POINT_STEAL_HP`,
--   `POINT_RESIST_SWORD`, `POINT_RESIST_TWOHAND`), this migration enforces a
--   STRICT, FAIL-CLOSED 5-PART FINGERPRINT before converting any row:
--     1. `vnum IN (74020, 74270, 74520, 74770, 75020, 75220, 75420, 75620)`
--     2. Exact `(attrtype0, attrvalue0)` tuple from `event_halloween_hair.quest`:
--          (59, 10) -> (54, 10)  [POINT_ATTBONUS_WARRIOR]
--          (60, 10) -> (55, 10)  [POINT_ATTBONUS_ASSASSIN]
--          (61, 10) -> (56, 10)  [POINT_ATTBONUS_SURA]
--          (62, 10) -> (57, 10)  [POINT_ATTBONUS_SHAMAN]
--          (63, 10) -> (53, 10)  [POINT_ATTBONUS_MONSTER]
--          (69, 1)  -> (119, 1)  [POINT_MAX_HP_PCT]
--          (70, 1)  -> (120, 1)  [POINT_MAX_SP_PCT]
--     3. `attrtype1..6 = 0` AND `attrvalue1..6 = 0` (quest only sets slot 0;
--        `ITEM_COSTUME` forbids attribute add/change scrolls in char_item.cpp).
--     4. `socket2 = 0` AND valid Halloween quest duration/expiry socket state:
--        - Unworn since quest grant: `socket1 = 0` AND
--          ((`attrtype0 IN (59,60,61,62,63) AND socket0 = 604800`) OR
--           (`attrtype0 = 69 AND socket0 = 21600`) OR
--           (`attrtype0 = 70 AND socket0 = 86400`))
--        - Equipped at least once: `socket1 >= 1 AND socket0 > 604800`
--     5. Any row on vnums 74020..75620 with `attrtype0 IN (59,60,61,62,63,69,70)`
--        that does NOT match conditions (2)-(4) (e.g. GM `/item_setattr` with
--        custom value, non-zero secondary slots, or `/i` command with socket0=0)
--        is classified as `AMBIGUOUS_DO_NOT_CONVERT` and is NEVER auto-updated.
--
-- MAINTENANCE / CACHE CONSISTENCY REQUIREMENT:
--   `db_r41023` caches `player.item` rows in memory (`CItemCache`).
--   This migration MUST be executed while `game_r41023` and `db_r41023` are
--   cleanly stopped (offline maintenance window) so that `db` cache flushed
--   all dirty items before SQL update and loads the migrated values on start.
--   Because `player.item` has no provenance column recording whether an item
--   was granted by `event_halloween_hair.quest` or created by a GM who manually
--   set a raw `EPointTypes` value (59..63, 69, 70) with the exact same
--   `(vnum, socket0..2, attrvalue0=10/1, attrtype1..6=0)` fingerprint,
--   UNATTENDED AUTOMATIC PRODUCTION MIGRATION IS NOT SAFE.
--   Therefore, Step 2a stages candidate rows into `player_item_halloween_rev51_backup`
--   with `approved_for_migration = 0` (modifying 0 rows in `player.item`).
--   Step 2b ONLY converts rows where a human operator has verified provenance
--   and explicitly set `approved_for_migration = 1`.
-- =============================================================================

-- -----------------------------------------------------------------------------
-- STEP 1: READ-ONLY DRY-RUN AUDIT (NO PII / NO OWNER_ID EXPOSED)
-- -----------------------------------------------------------------------------
SELECT
    i.`vnum`,
    i.`attrtype0`,
    i.`attrvalue0`,
    CASE
        WHEN (
            (
                (i.`attrtype0` IN (59, 60, 61, 62, 63) AND i.`attrvalue0` = 10)
                OR (i.`attrtype0` = 69 AND i.`attrvalue0` = 1)
                OR (i.`attrtype0` = 70 AND i.`attrvalue0` = 1)
            )
            AND i.`attrtype1` = 0 AND i.`attrvalue1` = 0
            AND i.`attrtype2` = 0 AND i.`attrvalue2` = 0
            AND i.`attrtype3` = 0 AND i.`attrvalue3` = 0
            AND i.`attrtype4` = 0 AND i.`attrvalue4` = 0
            AND i.`attrtype5` = 0 AND i.`attrvalue5` = 0
            AND i.`attrtype6` = 0 AND i.`attrvalue6` = 0
            AND i.`socket2` = 0
            AND (
                (i.`socket1` = 0 AND (
                    (i.`attrtype0` IN (59, 60, 61, 62, 63) AND i.`socket0` = 604800)
                    OR (i.`attrtype0` = 69 AND i.`socket0` = 21600)
                    OR (i.`attrtype0` = 70 AND i.`socket0` = 86400)
                ))
                OR (i.`socket1` >= 1 AND i.`socket0` > 604800)
            )
        ) THEN 'CANDIDATE_REQUIRES_HUMAN_APPROVAL'
        WHEN (
            (
                (i.`attrtype0` IN (53, 54, 55, 56, 57) AND i.`attrvalue0` = 10)
                OR (i.`attrtype0` IN (119, 120) AND i.`attrvalue0` = 1)
            )
            AND i.`attrtype1` = 0 AND i.`attrvalue1` = 0
            AND i.`attrtype2` = 0 AND i.`attrvalue2` = 0
            AND i.`attrtype3` = 0 AND i.`attrvalue3` = 0
            AND i.`attrtype4` = 0 AND i.`attrvalue4` = 0
            AND i.`attrtype5` = 0 AND i.`attrvalue5` = 0
            AND i.`attrtype6` = 0 AND i.`attrvalue6` = 0
        ) THEN 'ALREADY_VALID_POINT'
        WHEN i.`attrtype0` IN (59, 60, 61, 62, 63, 69, 70) THEN 'AMBIGUOUS_DO_NOT_CONVERT'
        ELSE 'OTHER_OR_UNSET'
    END AS `audit_classification`,
    COUNT(*) AS `item_count`
FROM `item` i
WHERE i.`vnum` IN (74020, 74270, 74520, 74770, 75020, 75220, 75420, 75620)
GROUP BY i.`vnum`, i.`attrtype0`, i.`attrvalue0`, `audit_classification`
ORDER BY i.`vnum`, `audit_classification`, i.`attrtype0`;

-- -----------------------------------------------------------------------------
-- STEP 2: BACKUP TABLE & HUMAN-APPROVED OPT-IN TRANSACTIONAL MIGRATION
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player_item_halloween_rev51_backup` (
    `item_id` bigint(20) unsigned NOT NULL,
    `vnum` int(11) unsigned NOT NULL,
    `window` enum('INVENTORY','EQUIPMENT','SAFEBOX','MALL','DRAGON_SOUL_INVENTORY','BELT_INVENTORY','GROUND','IKASHOP_OFFLINESHOP','IKASHOP_SAFEBOX','IKASHOP_AUCTION') NOT NULL,
    `pos` smallint(5) unsigned NOT NULL,
    `count` int(10) unsigned NOT NULL,
    `socket0` int(10) unsigned NOT NULL,
    `socket1` int(10) unsigned NOT NULL,
    `socket2` int(10) unsigned NOT NULL,
    `old_attrtype0` tinyint(3) unsigned NOT NULL,
    `old_attrvalue0` smallint(6) NOT NULL,
    `new_attrtype0` tinyint(3) unsigned NOT NULL,
    `new_attrvalue0` smallint(6) NOT NULL,
    `attrtype1` tinyint(3) unsigned NOT NULL,
    `attrvalue1` smallint(6) NOT NULL,
    `attrtype2` tinyint(3) unsigned NOT NULL,
    `attrvalue2` smallint(6) NOT NULL,
    `attrtype3` tinyint(3) unsigned NOT NULL,
    `attrvalue3` smallint(6) NOT NULL,
    `attrtype4` tinyint(3) unsigned NOT NULL,
    `attrvalue4` smallint(6) NOT NULL,
    `attrtype5` tinyint(3) unsigned NOT NULL,
    `attrvalue5` smallint(6) NOT NULL,
    `attrtype6` tinyint(3) unsigned NOT NULL,
    `attrvalue6` smallint(6) NOT NULL,
    `approved_for_migration` tinyint(1) unsigned NOT NULL DEFAULT 0,
    `migrated_at` datetime NOT NULL DEFAULT current_timestamp(),
    PRIMARY KEY (`item_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

START TRANSACTION;

-- 2a. Stage candidate rows into backup table with approved_for_migration = 0 (no item rows modified yet)
INSERT INTO `player_item_halloween_rev51_backup` (
    `item_id`, `vnum`, `window`, `pos`, `count`,
    `socket0`, `socket1`, `socket2`,
    `old_attrtype0`, `old_attrvalue0`,
    `new_attrtype0`, `new_attrvalue0`,
    `attrtype1`, `attrvalue1`,
    `attrtype2`, `attrvalue2`,
    `attrtype3`, `attrvalue3`,
    `attrtype4`, `attrvalue4`,
    `attrtype5`, `attrvalue5`,
    `attrtype6`, `attrvalue6`,
    `approved_for_migration`,
    `migrated_at`
)
SELECT
    i.`id`, i.`vnum`, i.`window`, i.`pos`, i.`count`,
    i.`socket0`, i.`socket1`, i.`socket2`,
    i.`attrtype0`, i.`attrvalue0`,
    CASE i.`attrtype0`
        WHEN 59 THEN 54
        WHEN 60 THEN 55
        WHEN 61 THEN 56
        WHEN 62 THEN 57
        WHEN 63 THEN 53
        WHEN 69 THEN 119
        WHEN 70 THEN 120
    END AS `new_attrtype0`,
    i.`attrvalue0` AS `new_attrvalue0`,
    i.`attrtype1`, i.`attrvalue1`,
    i.`attrtype2`, i.`attrvalue2`,
    i.`attrtype3`, i.`attrvalue3`,
    i.`attrtype4`, i.`attrvalue4`,
    i.`attrtype5`, i.`attrvalue5`,
    i.`attrtype6`, i.`attrvalue6`,
    0 AS `approved_for_migration`,
    NOW()
FROM `item` i
LEFT JOIN `player_item_halloween_rev51_backup` b ON b.`item_id` = i.`id`
WHERE b.`item_id` IS NULL
  AND i.`vnum` IN (74020, 74270, 74520, 74770, 75020, 75220, 75420, 75620)
  AND (
      (i.`attrtype0` IN (59, 60, 61, 62, 63) AND i.`attrvalue0` = 10)
      OR (i.`attrtype0` = 69 AND i.`attrvalue0` = 1)
      OR (i.`attrtype0` = 70 AND i.`attrvalue0` = 1)
  )
  AND i.`attrtype1` = 0 AND i.`attrvalue1` = 0
  AND i.`attrtype2` = 0 AND i.`attrvalue2` = 0
  AND i.`attrtype3` = 0 AND i.`attrvalue3` = 0
  AND i.`attrtype4` = 0 AND i.`attrvalue4` = 0
  AND i.`attrtype5` = 0 AND i.`attrvalue5` = 0
  AND i.`attrtype6` = 0 AND i.`attrvalue6` = 0
  AND i.`socket2` = 0
  AND (
      (i.`socket1` = 0 AND (
          (i.`attrtype0` IN (59, 60, 61, 62, 63) AND i.`socket0` = 604800)
          OR (i.`attrtype0` = 69 AND i.`socket0` = 21600)
          OR (i.`attrtype0` = 70 AND i.`socket0` = 86400)
      ))
      OR (i.`socket1` >= 1 AND i.`socket0` > 604800)
  );

-- HUMAN OPERATOR APPROVAL GATE:
-- After inspecting `SELECT * FROM player_item_halloween_rev51_backup WHERE approved_for_migration = 0;`
-- and cross-checking against GM/item logs to exclude any GM-crafted EPointTypes item,
-- explicitly approve verified quest-generated item_ids:
--   UPDATE `player_item_halloween_rev51_backup`
--      SET `approved_for_migration` = 1
--    WHERE `item_id` IN (/* verified quest-generated item_id list */);

-- 2b. Update ONLY rows backed up in `player_item_halloween_rev51_backup` with `approved_for_migration = 1`
UPDATE `item` i
INNER JOIN `player_item_halloween_rev51_backup` b ON b.`item_id` = i.`id`
SET i.`attrtype0` = b.`new_attrtype0`,
    b.`migrated_at` = NOW()
WHERE b.`approved_for_migration` = 1
  AND i.`vnum` = b.`vnum`
  AND i.`attrtype0` = b.`old_attrtype0`
  AND i.`attrvalue0` = b.`old_attrvalue0`
  AND i.`attrtype1` = 0 AND i.`attrvalue1` = 0
  AND i.`attrtype2` = 0 AND i.`attrvalue2` = 0
  AND i.`attrtype3` = 0 AND i.`attrvalue3` = 0
  AND i.`attrtype4` = 0 AND i.`attrvalue4` = 0
  AND i.`attrtype5` = 0 AND i.`attrvalue5` = 0
  AND i.`attrtype6` = 0 AND i.`attrvalue6` = 0;

COMMIT;

-- -----------------------------------------------------------------------------
-- STEP 3: POST-MIGRATION VERIFICATION (AGGREGATE SUMMARY, NO PII)
-- -----------------------------------------------------------------------------
SELECT
    COUNT(*) AS `total_backed_up`,
    SUM(CASE WHEN b.`approved_for_migration` = 1 THEN 1 ELSE 0 END) AS `human_approved_count`,
    SUM(CASE WHEN i.`attrtype0` = b.`new_attrtype0` AND i.`attrvalue0` = b.`new_attrvalue0` THEN 1 ELSE 0 END) AS `verified_converted_count`,
    SUM(CASE WHEN i.`attrtype0` = b.`old_attrtype0` THEN 1 ELSE 0 END) AS `unconverted_remaining_count`
FROM `player_item_halloween_rev51_backup` b
INNER JOIN `item` i ON i.`id` = b.`item_id`;

-- =============================================================================
-- STEP 4: DETERMINISTIC ROLLBACK PROCEDURE (EXECUTE ONLY IF ROLLBACK REQUIRED)
-- =============================================================================
-- START TRANSACTION;
-- UPDATE `item` i
-- INNER JOIN `player_item_halloween_rev51_backup` b ON b.`item_id` = i.`id`
-- SET i.`attrtype0` = b.`old_attrtype0`
-- WHERE i.`vnum` = b.`vnum`
--   AND i.`attrtype0` = b.`new_attrtype0`
--   AND i.`attrvalue0` = b.`new_attrvalue0`
--   AND i.`attrtype1` = b.`attrtype1` AND i.`attrvalue1` = b.`attrvalue1`
--   AND i.`attrtype2` = b.`attrtype2` AND i.`attrvalue2` = b.`attrvalue2`
--   AND i.`attrtype3` = b.`attrtype3` AND i.`attrvalue3` = b.`attrvalue3`
--   AND i.`attrtype4` = b.`attrtype4` AND i.`attrvalue4` = b.`attrvalue4`
--   AND i.`attrtype5` = b.`attrtype5` AND i.`attrvalue5` = b.`attrvalue5`
--   AND i.`attrtype6` = b.`attrtype6` AND i.`attrvalue6` = b.`attrvalue6`;
-- COMMIT;
-- =============================================================================
