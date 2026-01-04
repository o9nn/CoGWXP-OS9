/**
 * @file powershell_github.h
 * @brief PowerShell for GitHub Automation
 *
 * Provides automation for GitHub operations using PowerShell-like
 * cmdlet patterns for CI/CD, repository management, and DevOps.
 *
 * Based on: https://github.com/microsoft/PowerShellForGitHub
 *
 * @copyright CoGWXP-OS9 Project
 */

#ifndef COGWXP_POWERSHELL_GITHUB_H
#define COGWXP_POWERSHELL_GITHUB_H

#include "../orchestration.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Types and Structures
 *===========================================================================*/

/* Repository info */
typedef struct {
    uint64_t id;
    const char* name;
    const char* full_name;
    const char* description;
    const char* default_branch;
    bool is_private;
    bool is_fork;
    uint32_t stars;
    uint32_t forks;
    uint32_t open_issues;
    const char* language;
    int64_t created_at;
    int64_t updated_at;
} psgithub_repo_t;

/* Issue info */
typedef struct {
    uint64_t id;
    uint32_t number;
    const char* title;
    const char* body;
    const char* state;
    const char* author;
    const char** labels;
    size_t label_count;
    const char** assignees;
    size_t assignee_count;
    int64_t created_at;
    int64_t updated_at;
} psgithub_issue_t;

/* Pull request info */
typedef struct {
    uint64_t id;
    uint32_t number;
    const char* title;
    const char* body;
    const char* state;
    const char* author;
    const char* head_branch;
    const char* base_branch;
    bool mergeable;
    bool merged;
    uint32_t commits;
    uint32_t additions;
    uint32_t deletions;
    uint32_t changed_files;
    int64_t created_at;
    int64_t merged_at;
} psgithub_pr_t;

/* Workflow run */
typedef struct {
    uint64_t id;
    const char* name;
    const char* status;
    const char* conclusion;
    const char* head_branch;
    const char* head_sha;
    int64_t created_at;
    int64_t updated_at;
} psgithub_workflow_run_t;

/* Release info */
typedef struct {
    uint64_t id;
    const char* tag_name;
    const char* name;
    const char* body;
    bool draft;
    bool prerelease;
    const char* tarball_url;
    const char* zipball_url;
    int64_t created_at;
    int64_t published_at;
} psgithub_release_t;

/* Configuration */
typedef struct {
    const char* access_token;
    const char* api_url;           /* Default: https://api.github.com */
    const char* default_owner;
    const char* default_repo;
    uint32_t timeout_seconds;
    uint32_t retry_count;
    bool enable_caching;
} psgithub_config_t;

/* Statistics */
typedef struct {
    uint64_t api_calls;
    uint64_t rate_limit_remaining;
    uint64_t rate_limit_reset;
    uint64_t cache_hits;
    uint64_t cache_misses;
} psgithub_stats_t;

/* Context handle */
typedef struct psgithub_context* psgithub_context_t;

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

COGUTIL_API cog_result_t psgithub_init(
    const psgithub_config_t* config,
    psgithub_context_t* ctx
);

COGUTIL_API void psgithub_shutdown(psgithub_context_t ctx);

/*===========================================================================
 * Repository Operations (Get-GitHubRepository, New-GitHubRepository, etc.)
 *===========================================================================*/

COGUTIL_API cog_result_t psgithub_get_repository(
    psgithub_context_t ctx,
    const char* owner,
    const char* repo,
    psgithub_repo_t* info
);

COGUTIL_API cog_result_t psgithub_list_repositories(
    psgithub_context_t ctx,
    const char* owner,
    psgithub_repo_t** repos,
    size_t* count
);

COGUTIL_API cog_result_t psgithub_create_repository(
    psgithub_context_t ctx,
    const char* name,
    const char* description,
    bool is_private,
    char** repo_url
);

COGUTIL_API cog_result_t psgithub_fork_repository(
    psgithub_context_t ctx,
    const char* owner,
    const char* repo,
    char** fork_url
);

/*===========================================================================
 * Issue Operations (Get-GitHubIssue, New-GitHubIssue, etc.)
 *===========================================================================*/

COGUTIL_API cog_result_t psgithub_get_issue(
    psgithub_context_t ctx,
    const char* owner,
    const char* repo,
    uint32_t issue_number,
    psgithub_issue_t* issue
);

COGUTIL_API cog_result_t psgithub_list_issues(
    psgithub_context_t ctx,
    const char* owner,
    const char* repo,
    const char* state,      /* "open", "closed", "all" */
    const char* labels,     /* Comma-separated */
    psgithub_issue_t** issues,
    size_t* count
);

COGUTIL_API cog_result_t psgithub_create_issue(
    psgithub_context_t ctx,
    const char* owner,
    const char* repo,
    const char* title,
    const char* body,
    const char** labels,
    size_t label_count,
    uint32_t* issue_number
);

COGUTIL_API cog_result_t psgithub_update_issue(
    psgithub_context_t ctx,
    const char* owner,
    const char* repo,
    uint32_t issue_number,
    const char* title,
    const char* body,
    const char* state
);

COGUTIL_API cog_result_t psgithub_add_issue_comment(
    psgithub_context_t ctx,
    const char* owner,
    const char* repo,
    uint32_t issue_number,
    const char* comment
);

/*===========================================================================
 * Pull Request Operations (Get-GitHubPullRequest, New-GitHubPullRequest, etc.)
 *===========================================================================*/

COGUTIL_API cog_result_t psgithub_get_pull_request(
    psgithub_context_t ctx,
    const char* owner,
    const char* repo,
    uint32_t pr_number,
    psgithub_pr_t* pr
);

COGUTIL_API cog_result_t psgithub_list_pull_requests(
    psgithub_context_t ctx,
    const char* owner,
    const char* repo,
    const char* state,
    psgithub_pr_t** prs,
    size_t* count
);

COGUTIL_API cog_result_t psgithub_create_pull_request(
    psgithub_context_t ctx,
    const char* owner,
    const char* repo,
    const char* title,
    const char* body,
    const char* head,
    const char* base,
    uint32_t* pr_number
);

COGUTIL_API cog_result_t psgithub_merge_pull_request(
    psgithub_context_t ctx,
    const char* owner,
    const char* repo,
    uint32_t pr_number,
    const char* merge_method  /* "merge", "squash", "rebase" */
);

/*===========================================================================
 * Actions/Workflow Operations
 *===========================================================================*/

COGUTIL_API cog_result_t psgithub_list_workflow_runs(
    psgithub_context_t ctx,
    const char* owner,
    const char* repo,
    const char* workflow_id,
    psgithub_workflow_run_t** runs,
    size_t* count
);

COGUTIL_API cog_result_t psgithub_trigger_workflow(
    psgithub_context_t ctx,
    const char* owner,
    const char* repo,
    const char* workflow_id,
    const char* ref,
    const char* inputs_json
);

COGUTIL_API cog_result_t psgithub_cancel_workflow_run(
    psgithub_context_t ctx,
    const char* owner,
    const char* repo,
    uint64_t run_id
);

/*===========================================================================
 * Release Operations
 *===========================================================================*/

COGUTIL_API cog_result_t psgithub_list_releases(
    psgithub_context_t ctx,
    const char* owner,
    const char* repo,
    psgithub_release_t** releases,
    size_t* count
);

COGUTIL_API cog_result_t psgithub_create_release(
    psgithub_context_t ctx,
    const char* owner,
    const char* repo,
    const char* tag_name,
    const char* name,
    const char* body,
    bool draft,
    bool prerelease,
    uint64_t* release_id
);

/*===========================================================================
 * Content Operations
 *===========================================================================*/

COGUTIL_API cog_result_t psgithub_get_content(
    psgithub_context_t ctx,
    const char* owner,
    const char* repo,
    const char* path,
    const char* ref,
    char** content
);

COGUTIL_API cog_result_t psgithub_create_or_update_file(
    psgithub_context_t ctx,
    const char* owner,
    const char* repo,
    const char* path,
    const char* message,
    const char* content,
    const char* branch,
    const char* sha  /* NULL for new file */
);

/*===========================================================================
 * Statistics
 *===========================================================================*/

COGUTIL_API cog_result_t psgithub_get_stats(
    psgithub_context_t ctx,
    psgithub_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* COGWXP_POWERSHELL_GITHUB_H */
