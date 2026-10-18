-- =============================================================================
-- Metin2 MT2009 - Rev 47 Account & Password Security Schema Migration
-- Compatible with MySQL 8.4 LTS, MariaDB 11.4 LTS, and MariaDB 12.3 LTS
-- Strictly non-destructive: preserves all existing account rows and legacy
-- *SHA1(SHA1(password)) hashes for transparent in-place Argon2id upgrade.
-- =============================================================================

-- 1. Pre-Migration Backup Table (Idempotent Snapshot of Account Credentials)
CREATE TABLE IF NOT EXISTS `account_rev47_backup` LIKE `account`;

INSERT INTO `account_rev47_backup`
SELECT a.*
FROM `account` a
LEFT JOIN `account_rev47_backup` b ON b.`id` = a.`id`
WHERE b.`id` IS NULL;

-- 2. Normalize any legacy zero-dates on shop_expire before strict ALTER TABLE
UPDATE `account`
SET `shop_expire` = `create_time`
WHERE CAST(`shop_expire` AS CHAR(20)) = '0000-00-00 00:00:00';

-- 3. Widen `login` to VARCHAR(30) (LOGIN_MAX_LEN=30) and `password` to VARCHAR(255)
--    to store PHC-formatted Argon2id hashes ($argon2id$v=19$m=19456,t=2,p=1$<salt>$<tag>)
ALTER TABLE `account`
  MODIFY COLUMN `login` varchar(30) NOT NULL DEFAULT '' COMMENT 'LOGIN_MAX_LEN=30',
  MODIFY COLUMN `password` varchar(255) NOT NULL DEFAULT '' COMMENT 'PASSWD_MAX_LEN=16; Argon2id PHC / legacy *SHA1',
  MODIFY COLUMN `shop_expire` datetime NOT NULL DEFAULT current_timestamp();

-- =============================================================================
-- ROLLBACK & PRODUCTION SAFETY POLICY:
--   1. Never narrow `account`.`password` back to VARCHAR(45); preserve VARCHAR(255)
--      so 97-character Argon2id PHC hashes are never truncated.
--   2. Never run automatic or blind bulk UPDATE from `account_rev47_backup`
--      (and never auto-restore legacy SHA-1 hashes based on `last_play`), as
--      live accounts may have upgraded to Argon2id or changed passwords.
-- =============================================================================
